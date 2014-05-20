// debug infók (kommentezd, ha nem kell)
//~ #define CON_DEBUG

// csomagok dumpolása stdout-ra (kommentezd, ha nem kell)
//~ #define AMI_DEBUG_PACKET_DUMP

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ev.h>
#include <stdio.h> // TODO: kell ez?
#include <stdarg.h>
#include <sys/time.h> // gettimeofday()

#include "ami.h"
#include "debug.h"
#include "utlist.h"
#include "misc.h"
#include "logger.h"

// event rögzítése
void put_event (ami_event_t *event) {
    ami_event_t *event_copy = (ami_event_t*)malloc(sizeof(ami_event_t));
    if (!event_copy) {
        conft("can't allocate event: %s, event lost", strerror(errno));
        return;
    }

    //~ event tartalmának másolása az új event_copy számára lefoglalt területre.
    //~ Emiatt lesz thread-safe a várakozó sor és a callback hívás.
    memcpy(event_copy, event, sizeof(ami_event_t));

    // bedobjuk a listába az új eseményt
    DL_APPEND(event->ami->event_head, event_copy);

    //~ Esedékessé tesszük az azonnali (0 sec) időzítőt, aminek hatására az event
    //~ loop a callback futtatások után azonnal meghívja az invoke_events()
    //~ függvényt, ami meghivogatja a sorban álló eventekhez tartozó callback
    //~ eljárásokat. Majd a multithread fejlesztésénél ezen a ponton az
    //~ ev_timer_start() helyett az ev_async() függvénnyel kell jelezni a másik
    //~ szálban futó event loop-nak, hogy dolog van.
    ev_timer_start(event->ami->loop, &event->ami->need_event_processing);
}

/* Felépítjük az event->field string tömböt, amiben az Asterisk által
küldött "változó: érték" párokat mentjük el úgy, hogy "változó", "érték",
"változó", "érték", ... A tömböt az ami->inbuf mutatókkal való
feldarabolásával és NULL byteok elhelyezésével kapjuk.

A sorvégeket lezárhatja \r\n és \n is egyaránt.
A legutolsó sor végét nem kötelező lezárni.

Ha az ami->inbuf tartalma:
Response: Success
Message: Authentication accepted

Akkor az ami->field:
{"Respone","Success","Message","Authentication accepted"}

field              string tömb
max_field_size     maximum ennyi darab string rakható a field tömbbe
field_len          annyi lesz az értéke, ahány elem bekerült a field tömbbe
data               innen olvassuk az adatokat és ezt a buffert daraboljuk fel és zárjuk le NULL-al
data_size          data mérete
*/

//~ TODO: Hibás a függvény működése, ha a **field tömbünk mérete kicsi és a
//~ feldarabolás során nem férnek el benne a tokenek. Nincs segfault meg
//~ memóriahiba, hanem csak annyi történik, hogy az utolsó változó-érték pár
//~ értéke megkapja sortörésekkel együtt a maradék buffert. Ezt úgy lehetne
//~ megoldani, hogy a függvény nem bal-jobb oldalt vizsgál, hanem egy for ciklus
//~ NULL-ra állítja a ": " és a "\r" és "\n" karaktereket a teljes data-ban, majd
//~ csak ezután következne a feldarabolás mutatókkal.
//~
//~ aug 21: A fent leirt hiba sz'tem nem hiba.
//~
//~ aug 29: mi lenne, ha az utolsó változó-érték pár nem kapná meg a teljes buffert?
//~ vagy eleve netsocket_disconnect és feltakarítás kellene ide?
//~
//~ A függvény nem kezeli azt az esetet, amikor az AMI változó-érték párnak nincs
//~ értéke és nincs a változó utáni kettőspont után szóköz, hanem egyből újsor
//~ karakter. Ilyen eset áll fenn az RTCPSent esemény ReportBlock változójában.
void tokenize_field (int *field, int max_field_size, int *field_len, char *data, int data_size) {
    enum {
        LEFT,
        RIGHT,
    } inexpr = LEFT;

    int len = 0; // visszatéréskor ezt mentjük el a *field_len -be
    field[len++] = 0; // első pozíció a data legeleje, tehát 0
    int i;

    // összes \r karakter nullázása
    for (i = 0; i < data_size; i++)
        if (data[i] == '\r')
            data[i] = '\0';

    for (i = 0; i < data_size && len < max_field_size; i++) {
        if (inexpr == LEFT) { // ": " bal oldalán vagyunk, változó
            if (data[i] == ':' && data[i+1] == ' ') {
                data[i] = '\0';
                data[i+1] = '\0';
                i += 2;
                field[len++] = i;
                inexpr = RIGHT;
            }
        }

        if (inexpr == RIGHT) { // ": " jobb oldalán vagyunk, érték
            if (data[i] == '\n') {
                data[i] = '\0';
                i += 1;
                field[len++] = i;
                inexpr = LEFT;
            }
        }
    }

    if (inexpr == LEFT)
        len--;

    *field_len = len;

    // AMI bal és jobb értékek dumpolása
#ifdef AMI_DEBUG_PACKET_DUMP
    int z;
    for (z = 0; z < len; z++)
        printf("tokenize_field ### %d - (%s)\n", z, &data[field[z]]);
    printf("\n");
#endif
}

static char *type2name (enum ami_event_type type) {
    switch (type) {
        case AMI_EVENT:        return "EVENT"       ; break;
        case AMI_RESPONSE:     return "RESPONSE"    ; break;
        case AMI_CLIRESPONSE:  return "CLIRESPONSE" ; break;
        case AMI_CONNECT:      return "CONNECT"     ; break;
        case AMI_DISCONNECT:   return "DISCONNECT"  ; break;
        default: return "UNKNOWN";
    }
}

// belső esemény kiváltása (AMI_CONNECT, AMI_DISCONNECT, stb...)
static void generate_local_event (ami_t *ami, enum ami_event_type type, const char *fmt, ...) {
    ami_event_t event_tmp; // ideiglenes event // TODO: ha működik, akkor bevezetni az ami->event_tmp helyett lent is
    ami_event_t *event = &event_tmp;
    bzero(event, sizeof(*event));

    //~ char buf[AMI_BUFSIZ];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(event->data, sizeof(event->data), fmt, ap);
    va_end(ap);
    event->data[AMI_BUFSIZ-1] = '\0'; // védelem // TODO: kell ez?
    event->data_size = strlen(event->data);

//~ printf("~~~ %s ~~~\n", event->data);

    tokenize_field(
        event->field,
        sizeof(event->field) / sizeof(char*) - 1,
        &event->field_size,
        event->data,
        event->data_size
    );

    ami_event_list_t *el = NULL;
    // végigmegyünk a regisztrált eseményeken
    DL_FOREACH(ami->ami_event_list_head, el) {
        if (el->type == type) {
            event->callback = el->callback;
            event->userdata = el->userdata;
            event->regby_file = el->regby_file;
            event->regby_line = el->regby_line;
            event->regby_function = el->regby_function;
            event->regby_cbname = el->regby_cbname;
            event->regby_udname = el->regby_udname;
            event->ami = ami;
            event->type = el->type;
            put_event(event);
        }
    }
}

static void parse_cliresponse (ami_t *ami, int actionid, char *buf, int size) {
    printf("***** CLI RESPONSE START *****\n");
    printf("***** ActionID = %d\n", actionid);
    int i;
    for (i = 0; i < size; i++) {
        putchar(buf[i]);
    }
    printf("***** CLI RESPONSE END *****\n\n");
}

// bejövő Response es Event feldolgozása
static void parse_input (ami_t *ami, char *buf, int size) {
    ami_event_t *event = &ami->event_tmp;
    bzero(event, sizeof(*event));

    memcpy(event->data, buf, size);
    event->data_size = size;

    tokenize_field(
        event->field,
        sizeof(event->field) / sizeof(char*) - 1,
        &event->field_size,
        event->data,
        size
    );

    char *var_response = ami_getvar(event, "Response");
    char *var_event = ami_getvar(event, "Event");
    /* * * RESPONSE * * */
    if (!strlen(var_event)) {
        char *action_id_str = ami_getvar(event, "ActionID");
        if (action_id_str == NULL) {
            con_debug("Missing ActionID in Response!");
            return;
        }
        event->action_id = atoi(action_id_str);

        if (!strcmp(var_response, "Success")) {
            event->success = 1;
        } else if (!strcmp(var_response, "Error")) {
            event->success = 0;
        } else {
            con_debug("Unknown Response value: %s", var_response);
            return;
        }

        con_debug("RESPONSE - success = %d, action_id = %d", event->success, event->action_id);

        ami_event_list_t *el = NULL;
        ami_event_list_t *eltmp = NULL;
        DL_FOREACH_SAFE(ami->ami_event_list_head, el, eltmp) {
            if (el->type != AMI_RESPONSE) // csak az AMI_RESPONSE típusú eseményeket vizsgáljuk
                continue;
            // event->action_id  - Asterisktől érkezett ActionID
            // el->action_id     - adatbázisban szereplő ActionID
            if (event->action_id == el->action_id) {
                event->callback = el->callback;
                event->userdata = el->userdata;
                event->regby_file = el->regby_file;
                event->regby_line = el->regby_line;
                event->regby_function = el->regby_function;
                event->regby_cbname = el->regby_cbname;
                event->regby_udname = el->regby_udname;
                event->ami = ami;
                event->type = AMI_RESPONSE;
                put_event(event);
                DL_DELETE(ami->ami_event_list_head, el);
                free(el);
                return;
            }
        }
        con_debug("Received ActionID=%d, but %d not found in ami_event_list_head!", event->action_id, event->action_id);

    /* * * EVENT * * */
    } else {
//~ printf("##### PARSE_INPUT EVENT #####\n");
        ami_event_list_t *el;
        // végigmegyünk a regisztrált eseményeken
        DL_FOREACH(ami->ami_event_list_head, el) {
            if (el->type != AMI_EVENT) // csak az AMI_EVENT típusú eseményeket vizsgáljuk
                continue;
            // regisztrációban definiált változó=érték párok száma
            int need_found = el->field_size / 2; // minden találatnál dekrementálva lesz
//~ printf(" REG need_found=%d allevents=%d by %s:%d\n", need_found, el->allevents, el->regby_file, el->regby_line);
            if (need_found || el->allevents) { // ha van mit keresnünk
                int n, i;
                // végigmegyünk a regisztráció változó=érték párjain
                for (n = 0; n < el->field_size; n += 2) {
//~ printf("  _reg_ %s=%s\n", &el->data[el->field[n]], &el->data[el->field[n+1]]);
                    // végigmegyünk a bejövő csomag változó=érték párjain
                    for (i = 0; i < event->field_size; i += 2) {
//~ printf("   _eve_ %s=%s\n", &event->data[event->field[i]], &event->data[event->field[i+1]]);
                        // ha egyezik a regisztrált változó neve a csomag változó nevével
                        if (!strcmp(&el->data[el->field[n]], &event->data[event->field[i]])) {
                            // ha egyezik a regisztrált változó értéke a csomag változó értékével
                            if (!strcmp(&el->data[el->field[n+1]], &event->data[event->field[i+1]])) {
//~ printf("      !found\n");
                                need_found--;
                            }
                        }
                    }
                }
//~ printf(" FIN need_found=%d\n", need_found);
                // ha minden változó megtalálható volt és mindegyik értéke egyezett
                // vagy "*" volt megadva a regisztrációnál (allevents)
                if (need_found == 0 || el->allevents) {
                    event->callback = el->callback;
                    event->userdata = el->userdata;
                    event->regby_file = el->regby_file;
                    event->regby_line = el->regby_line;
                    event->regby_function = el->regby_function;
                    event->regby_cbname = el->regby_cbname;
                    event->regby_udname = el->regby_udname;
                    event->type = AMI_EVENT;
                    event->ami = ami;
                    put_event(event);
                }
            }
        }
    }
}

static void response_login (ami_event_t *response) {
    ami_t *ami = response->ami;

    con_debug("auth reply: success=%d %s (by %s() %s:%d)",
        response->success,
        ami_getvar(response, "Message"),
        response->regby_function,
        response->regby_file,
        response->regby_line
    );

    if (!response->ami->authenticated) {
        if (response->success) { // AUTH accepted
            response->ami->authenticated = 1;
            // TODO: itt kell a connect timeout időzítőt törölni
            generate_local_event(ami,
                AMI_CONNECT,
                "Host: %s\nIP: %s\nPort: %d",
                ami->host,
                ami->netsocket->ip,
                ami->port);
        } else { // AUTH failed
            netsocket_disconnect_withevent(response->ami->netsocket, "Authentication failed");
        }
    }
}

static void process_input (ami_t *ami) {
#ifdef AMI_DEBUG_PACKET_DUMP
    int pdi;
    printf("----- NETSOCKET INBUF START -----\n");
    for (pdi = 0; pdi < ami->netsocket->inbuf_len; pdi++)
        putchar(ami->netsocket->inbuf[pdi]);
    printf("----- NETSOCKET INBUF END -----\n");
#endif

/*
      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20
      E  v  e  n  t  :     D  i  a  l \r \n \r \n  A  c  t  i  o  n
     \r \n \r \n

    "Response: Follows\r\nPrivilege: Command\r\nActionID: %d\r\n"
    "--END COMMAND--\r\n\r\n"
*/

    // byte-onként végigmegyünk a netsocket bufferen
    int i;
    for (i = 0; i < ami->netsocket->inbuf_len; i++) {
        // buffer overflow védelem
        if (ami->inbuf_pos >= sizeof(ami->inbuf) - 1) {
            printf("ELFOGYOTT A BUFFER!!!\n"); // TODO: netsocket_disconnect és feltakarítás
            break;
        }

        // byte másolása netsocketből ami->inbuf -ba
        ami->inbuf[ami->inbuf_pos] = ami->netsocket->inbuf[i];

        /* AMI header vizsgalata. Biztonsagi okokbol ha mar authentikalt
        allapotban vagyunk, akkor ezt a vizsgalatot kihagyjuk */
        if (!ami->authenticated &&
            (!strcmp(ami->inbuf, "Asterisk Call Manager/1.1\r\n") ||
            !strcmp(ami->inbuf, "Asterisk Call Manager/1.0\r\n")))
        {
            bzero(ami->inbuf, sizeof(ami->inbuf));
            ami->inbuf_pos = 0;
            con_debug("Received \"Asterisk Call Manager\" header, sending auth...");
            ami_action(ami, response_login, NULL,
                       "Action: Login\nUsername: %s\nSecret: %s\n",
                       ami->username, ami->secret);
            bzero(ami->inbuf, sizeof(ami->inbuf));
            ami->inbuf_pos = 0;
            return;
        }

        // ha épp egy "Response: Follows" belsejében vagyunk
        if (ami->cli_actionid > 0) {
            // keressük a csomag végét
            #define Q "--END COMMAND--\r\n\r\n"
            #define QSIZE 19
            if (ami->inbuf_pos >= QSIZE - 1) {
                if (!strncmp(Q, &ami->inbuf[ami->inbuf_pos - (QSIZE - 1)], QSIZE)) {
                    // megtaláltuk, mehet a feldolgozóba
                    parse_cliresponse(ami, ami->cli_actionid, ami->inbuf, ami->inbuf_pos - 18);
                    bzero(ami->inbuf, sizeof(ami->inbuf));
                    ami->inbuf_pos = 0;
                    ami->cli_actionid = 0;
                    continue;
                }
            }
            #undef Q
            #undef QSIZE

            // folytatjuk tovább a beolvasást
            ami->inbuf_pos++;
            continue;
        }

        /* Egy "Action: Command" csomagra (ami_cli() okozza) egy "Response:
        Follows" válaszcsomag érkezik. Az ilyen csomagoknak speciális
        formátuma van, ezért ezeket teljesen külön kell kezelni. Ezek a
        csomagok a parse_input() helyett a parse_cliresponse() függvénynek
        kerülnek át feldolgozásra. Az alábbi sscanf() megoldás megvizsgálja,
        hogy az éppen beolvasás alatt álló csomag ilyen speciális "Response:
        Follows" csomag lesz -e, illetve kiszedi belőle az ActionID-t. A
        sscanf() az ami->inbuf baloldali illeszkedését vizsgálja és ha
        tudja, akkor az ami->cli_actionid változóba menti el a kapott
        ActionID-t. */
        if (sscanf(ami->inbuf, "Response: Follows\r\nPrivilege: Command\r\nActionID: %d\r\n", &ami->cli_actionid) > 0) {
            // az ami->inbuf jobboldali illeszkedését az strncmp()-vel vizsgáljuk
            #define Q "\r\n"
            #define QSIZE 2 // Q mérete
            if (ami->inbuf_pos >= QSIZE - 1) {
                // ha illeszkedik jobbról a \r\n, akkor tovább olvassuk az adatokat
                if (!strncmp(Q, &ami->inbuf[ami->inbuf_pos - (QSIZE - 1)], QSIZE)) {
                    bzero(ami->inbuf, sizeof(ami->inbuf));
                    ami->inbuf_pos = 0;
                    continue; // ezen a ponton az ami->cli_actionid -ben ott figyel az ActionID
                }
            }
            #undef Q
            #undef QSIZE

            /* Ide akkor kerülünk, ha a sscanf() (bal oldal) illeszkedett, de az
            strncmp() (jobb oldal) nem. Ebben az esetben elképzelhető, hogy
            a sscanf() hibásan kiolvassa az ActionID egy töredékét, ezért
            biztos ami biztos, kinullázzuk. ezen a ponton lehetséges, hogy a
            sscanf() az ActinID csak egy töredékét szedte ki, ezért nullázunk. */
            ami->cli_actionid = 0;

            // folytatjuk tovább a beolvasást
            ami->inbuf_pos++;
            continue;
        }

        /* Ha van elég adat, hogy az ami->inbuf -ban 3 byte-ot visszaléphessünk,
        akkor megvizsgáljuk, hogy vajon éppen egy csomag lezárásánál állunk
        -e, azaz az ami->inbuf legutolsó 4 byte-ja megegyezik ezzel:
        "\r\n\r\n". Ha igen, akkor az azt jelenti, hogy az ami->inbuf pont
        egy teljes csomagot tartalmaz, amit elküldünk a parse_input()-nak,
        majd kinullázzuk a teljes ami->inbuf buffert és az ami->inbuf_pos
        pozicionáló változót. Ezután folytatjuk a következő csomag byte-
        onkénti olvasását. Ha már nincs a netsocket->inbuf -ban feldolgozandó
        cucc, akkor a for ciklus kilép és majd a következő körben
        folytatódik az olvasás. */

        /* TODO: Logikailag nem korrekt ez a megoldás! Segfault veszély
        ami_event_unregister() után. A process_input() és parse_input()
        páros a teljes netsocket->inbuf -ban lévő cuccot egyetlen egy körben
        feldolgozza. Ebben egyszerre több csomag is lehet. Az érdekes
        eseményeket a put_event() a saját listájába tolja és csak a
        következő körben lesz callback hívás. Tegyük fel, hogy egyszerre 2
        csomag érkezik. Az első csomagban lévő esemény callback függvénye
        megrendel egy új eseményt, amire történetesen pont a második csomag
        illeszkedne. De mivel a megrendelés előtt már megtörtént az
        összehasonlítás, szűrés és a futtatandó események kiválasztása,
        ezért erről a második eseményről le fog maradni a hívó. Másképpen
        szólva egy-egy megrendelésnek (vagy lemondásnak) csak a teljes
        put_event() által karbantartott lista (callback lista) lefuttatása
        után lesz hatása. Ez eseményről való lemaradást okozhat, illetve
        lemondásnál segfaultot is, ugyanis ha történik egy
        ami_event_unregister() akkor ezután még a put_event() által
        karbantartott listából lefuthat a (már lemondott) callback. Ötlet:
        valami olyan megoldás kéne, hogy még itt a process_input /
        parse_input szintjén ha fennakad a szűrőn egy esemény, akkor a
        put_event() regisztráció után álljon le a parse_input() és az event
        loop hívja meg a need_event_processing-et. És majd csak ezután
        folytatódjon a parse_input() vizsgálódása. Vaaagy... egy merészebb
        ötlet. A bejövő AMI buffer visszamenőleg addig legyen eltárolva,
        amíg az invoke_events még foglalkozik a callback hívásokkal.
        Multithread környezetben az invoke_events a megrendelő szálában fog
        futni. Elképzelhető, hogy az invoke_events-ből kellene vizsgálni
        azt, hogy az éppen bejövő AMI eseményt kell -e futtatni.  */

        #define Q "\r\n\r\n"
        #define QSIZE 4
        if (ami->inbuf_pos >= QSIZE - 1) {
            if (!strncmp(Q, &ami->inbuf[ami->inbuf_pos - (QSIZE - 1)], QSIZE)) {
                parse_input(ami, ami->inbuf, ami->inbuf_pos - 1); // -1 azért, hogy ne menjen át a legutolsó \r\n-ből az \r (érthető?)
                bzero(ami->inbuf, sizeof(ami->inbuf));
                ami->inbuf_pos = 0;
                continue; // ne jusson el az ami->inbuf_pos++ -ig :)
            }
        }
        #undef Q
        #undef QSIZE
        ami->inbuf_pos++;
    }
}

static void netsocket_callback (netsocket_t *netsocket, int event) {
    ami_t *ami = netsocket->userdata;
    int was_authenticated = 0;

    switch (event) {
        case NETSOCKET_EVENT_CONNECT:
            con_debug("Connected to %s (%s) port %d",
                netsocket->host,
                netsocket->ip,
                netsocket->port
            );
            break;

        case NETSOCKET_EVENT_DISCONNECT:
            was_authenticated = ami->authenticated;

            // TODO: itt kell alaphelyzetbe állítani az ami-t.
            // disconnect esemény szétkűrtölése előtt
            ami->authenticated = 0;

            generate_local_event(ami,
                AMI_DISCONNECT,
                "Host: %s\nIP: %s\nPort: %d\nReason: %s\nWasAuthenticated: %d",
                netsocket->host,
                (netsocket->ip) ? netsocket->ip : "",
                netsocket->port,
                netsocket->disconnect_reason,
                was_authenticated
            );

            if (netsocket->connected) {
                con_debug("Disconnected from %s: %s",
                    netsocket->host,
                    netsocket->disconnect_reason
                );
            } else {
                con_debug("Can't connect to %s[%s]:%d %s",
                    netsocket->host,
                    (netsocket->ip) ? netsocket->ip : "",
                    netsocket->port,
                    netsocket->disconnect_reason
                );
            }
            break;

        case NETSOCKET_EVENT_READ:
            process_input(ami);
            break;
    }
}

// hívja az ami->need_event_processing azonnali időzítő
static void invoke_events (EV_P_ ev_io *w, int revents) {
    ami_t *ami = w->data;

    ami_event_t *event, *tmp;
    DL_FOREACH_SAFE(ami->event_head, event, tmp) {
        if (event->callback != NULL) {
            con_debug("call %s()", event->regby_cbname);
            event->callback(event);
            con_debug("end %s()", event->regby_cbname);
        }
        DL_DELETE(ami->event_head, event);
        free(event);
    }
}

// 6 byte-os random hexa stringet masol az ami->uuid bufferbe
// TODO: egy rendes, unique ID-t visszaado fuggvenyt irni ehelyett a random vacak helyett
// pl. az util-linux-ng csomagban levo libuuid segitsegevel
static void generate_uuid (char *dst, size_t size) {
    struct timeval tv;
    int num;
    char tmp[16];

    gettimeofday(&tv, NULL);
    srand(tv.tv_usec * tv.tv_sec);
    num = rand();
    snprintf(tmp, sizeof(tmp), "%x", num);
    tmp[6] = '\0';
    strncpy(dst, tmp, size);
}

void connect_delayed (EV_P_ ev_io *w, int revents) {
    ami_t *ami = w->data;
    con_debug("invoked connect by timer");
    ev_timer_stop(ami->loop, &ami->t_connect_delayed);
    ami_connect(ami);
}

// delay: millisec
void ami_connect_delayed (ami_t *ami, int delay) {
    con_debug("connect after %d ms ...", delay);
    ev_timer_stop(ami->loop, &ami->t_connect_delayed);
    ev_timer_set(&ami->t_connect_delayed, (float)((float)delay / (float)1000), 0);
    ev_timer_start(ami->loop, &ami->t_connect_delayed);
}

ami_t *ami_new (struct ev_loop *loop) {
    ami_t *ami = malloc(sizeof(*ami));
    if (ami == NULL) {
        con_debug("ami_new() returned NULL");
        return NULL;
    }
    bzero(ami, sizeof(*ami)); // minden NULL

    // AMI UUID
    generate_uuid(ami->uuid, sizeof(ami->uuid));

    // ha meg van adva a loop paraméter, akkor azt használjuk eseménykezelőnek
    // ellenkező esetben az alapértelmezett eseménykezelőt
    ami->loop = (loop != NULL) ? loop : ev_default_loop(0);

    // default értékek
    strncpy(ami->host, AMI_DEFAULT_HOST, sizeof(ami->host) - 1);
    ami->port = AMI_DEFAULT_PORT;

    if (!(ami->netsocket = netsocket_new(netsocket_callback, ami, ami->loop))) {
        con_debug("netsocket_new returned NULL");
    }
    ami->netsocket->host = AMI_DEFAULT_HOST;
    ami->netsocket->port = AMI_DEFAULT_PORT;

    ami->need_event_processing.data = ami; // ami objektum így kerül az invoke_events-be
    ev_timer_init(&ami->need_event_processing, (void*)invoke_events, 0, 0);

    ami->t_connect_delayed.data = ami;
    ev_timer_init(&ami->t_connect_delayed, (void*)connect_delayed, 0, 0);

    return ami;
}

void ami_destroy(ami_t *ami) {
    netsocket_destroy(ami->netsocket);
}

void ami_credentials (ami_t *ami, const char *username, const char *secret, const char *host, const char *port) {
    if (username != NULL)
        strncpy(ami->username, username, sizeof(ami->username) - 1);

    if (secret != NULL)
        strncpy(ami->secret, secret, sizeof(ami->secret) - 1);

    if (host != NULL)
        strncpy(ami->host, host, sizeof(ami->host) - 1);

    if (port != NULL) {
        int port_tmp = atoi(port);
        if (port_tmp > 0 || port_tmp < 65536)
            ami->port = port_tmp;
    }
}

void ami_connect (ami_t *ami) {
    ami->netsocket->host = ami->host;
    ami->netsocket->port = ami->port;
    netsocket_connect(ami->netsocket);
}

int ami_printf (ami_t *ami, const char *fmt, ...) {
    char buf[AMI_BUFSIZ];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    int field[AMI_FIELD_SIZE];
    int field_size;
    tokenize_field(
        field,
        sizeof(field) / sizeof(char*) - 1,
        &field_size,
        buf,
        strlen(buf)
    );

    char packet[AMI_BUFSIZ];
    int i;
    strcpy(packet, "");
    for (i = 0; i < field_size; i += 2)
        concatf(packet, "%s: %s\r\n", &buf[field[i]], &buf[field[i+1]]);
    concat(packet, "\r\n");

    if (ami->netsocket != NULL) {
#ifdef AMI_DEBUG_PACKET_DUMP
        printf("----- NETSOCKET WRITE START ------\n");
        printf("%s", packet);
        printf("----- NETSOCKET WRITE END ------\n");
#endif
        return netsocket_printf(ami->netsocket, "%s", packet);
    } else {
        return -1;
    }
}

ami_event_list_t *_ami_action (ami_t *ami, void *callback, void *userdata, char *file, int line, const char *function, const char *cbname, const char *udname, const char *fmt, ...) {
    char buf[AMI_BUFSIZ];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[AMI_BUFSIZ-1] = '\0'; // védelem

    if (callback != NULL) {
        ami_event_list_t *el = malloc(sizeof(ami_event_list_t));
        bzero(el, sizeof(*el)); // NULL, NULL, NULL :)
        el->callback = callback;
        el->userdata = userdata;
        el->type = AMI_RESPONSE;
        el->regby_file = file;
        el->regby_line = line;
        el->regby_function = function;
        el->regby_cbname = cbname;
        el->regby_udname = udname;
        ami->action_id++; // új ActionID
        el->action_id = ami->action_id;
        ami_printf(ami, "ActionID: %d\n%s", ami->action_id, buf);
        con_debug("registered action #%d, callback: %s()", el->action_id, el->regby_cbname);
        DL_APPEND(ami->ami_event_list_head, el);
        return el;
    } else {
        ami_printf(ami, "%s", buf);
        return NULL;
    }
}

ami_event_list_t *_ami_event_register (ami_t *ami, void *callback, void *userdata, char *file, int line, const char *function, const char *cbname, const char *udname, const char *fmt, ...) {
    ami_event_list_t *el = malloc(sizeof(ami_event_list_t));
    bzero(el, sizeof(*el)); // NULL, NULL, NULL :)

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(el->data, sizeof(el->data), fmt, ap);
    va_end(ap);

    el->callback = callback;
    el->userdata = userdata;
    el->regby_file = file;
    el->regby_line = line;
    el->regby_function = function;
    el->regby_cbname = cbname;
    el->regby_udname = udname;

    // belső esemény: Connect
    if (!strcmp(el->data, "Connect")) {
        el->type = AMI_CONNECT;

    // belső esemény: Disconnect
    } else if (!strcmp(el->data, "Disconnect")) {
        el->type = AMI_DISCONNECT;

    // Minden Asterisk esemény szűrés nélkül
    } else if (!strcmp(el->data, "*")) {
        el->type = AMI_EVENT;
        el->allevents = 1;

    // Asterisk esemény, feltételek feldarabolása
    } else {
        el->type = AMI_EVENT;
        tokenize_field(
            el->field,
            sizeof(el->field) / sizeof(char*) - 1,
            &el->field_size,
            el->data,
            sizeof(el->data)
        );
    }

    DL_APPEND(ami->ami_event_list_head, el);
    con_debug("EVENT registered, callback: %s by %s() in %s line %d",
              el->regby_cbname, el->regby_function, el->regby_file, el->regby_line);

    return el;
}

void ami_event_unregister(ami_t *ami, ami_event_list_t *el) {
    if (el == NULL) {
        con_debug("attempting to unregister NULL pointer event!");
        return;
    }
    con_debug("EVENT unregistered, callback: %s()", el->regby_cbname);
    DL_DELETE(ami->ami_event_list_head, el);
    free(el);
}

void ami_event_dump (ami_event_t *event) {
    printf(
        "Incoming %s /0x%lx/\n"
        "  Registered by %s() in %s line %d\n"
        "  Callback: %s() /0x%lx/, Userdata: %s /0x%lx/\n"
        "  success=%d action_id=%d data_size=%d field_size=%d\n"
        , type2name(event->type), (unsigned long)event
        , event->regby_function, event->regby_file, event->regby_line
        , event->regby_cbname, (unsigned long)event->callback, event->regby_udname, (unsigned long)event->userdata
        , event->success, event->action_id, event->data_size, event->field_size
    );
    int i;
    for (i = 0; i < event->field_size; i += 2)
        printf("    %-16s %s\n", &event->data[event->field[i]], &event->data[event->field[i+1]]);
    printf("\n");
}

void ami_dump_event_list_element (ami_event_list_t *el) {
    printf(
        "Registered %s /0x%lx/\n"
        "  Registered by %s() in %s line %d\n"
        "  Callback: %s() /0x%lx/, Userdata: %s /0x%lx/\n"
        "  action_id=%d field_size=%d\n"
        , type2name(el->type), (unsigned long)el
        , el->regby_function, el->regby_file, el->regby_line
        , el->regby_cbname, (unsigned long)el->callback, el->regby_udname, (unsigned long)el->userdata
        , el->action_id, el->field_size
    );
    int i;
    for (i = 0; i < el->field_size; i += 2)
        printf("    %-16s %s\n", &el->data[el->field[i]], &el->data[el->field[i+1]]);
    printf("\n");
}

void ami_dump_lists (ami_t *ami) {
    printf("** REGISTERED AMI EVENTS **\n");
    ami_event_list_t *el;
    DL_FOREACH(ami->ami_event_list_head, el)
        ami_dump_event_list_element(el);
}

char *ami_getvar (ami_event_t *event, char *var) {
    int i;
    for (i = 0; i < event->field_size; i += 2) {
        if (!strcmp(&event->data[event->field[i]], var)) {
            if (&event->data[event->field[i+1]] != NULL) {
                return &event->data[event->field[i+1]];
            } else {
                return "";
            }
        }
    }
    return "";
}

void ami_strncpy (ami_event_t *event, char *dest, char *var, size_t maxsize) {

}

