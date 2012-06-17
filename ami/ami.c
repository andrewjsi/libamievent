#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ev.h>
#include <stdio.h> // TODO: kell ez?
#include <stdarg.h>

#include "ami.h"
#include "debug.h"

#define CON_DEBUG
#include "logger.h"

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
//~ NULL-ra állítja a ": " és a "\r" és "\n" karaktereket a teljes data-ban. Majd
//~ ezután következne a feldarabolás mutatókkal.
void tokenize_field (char **field, int max_field_size, int *field_len, char *data, int data_size) {
	enum {
		LEFT,
		RIGHT,
	} inexpr = LEFT;

	int len = 0; // visszatéréskor ezt mentjük el a *field_len -be
	field[len++] = data;
	int i;
	for (i = 0; i < data_size && len < max_field_size; i++) {
		if (data[i] == '\r') {
			data[i] = '\0';
			continue;
		}
		if (inexpr == LEFT) { // ": " bal oldalán vagyunk, változó
			if (data[i] == ':' && data[i+1] == ' ') {
				data[i] = '\0';
				data[i+1] = '\0';
				i += 2;
				field[len++] = data + i;
				inexpr = RIGHT;
			}
		}

		if (inexpr == RIGHT) { // ": " jobb oldalán vagyunk, érték
			if (data[i] == '\n') {
				data[i] = '\0';
				i += 1;
				field[len++] = data + i;
				inexpr = LEFT;
			}
		}
	}

	if (inexpr == LEFT)
		len--;

	*field_len = len;

	int z;
	for (z = 0; z < len; z++) {
		printf("tokenize_field ### %d - %s\n", z, field[z]);
	}
}


// teszt kedvéért
static void parse_input (ami_t *ami, char *buf, int size) {
	ami_event_t *event;
	event = &ami->event;
	bzero(event, sizeof(ami->event)); // az előző ami->event kinullázása

	tokenize_field(
		event->field,
		sizeof(event->field) / sizeof(char*) - 1,
		&event->field_size,
		buf,
		size
	);

	// event vizsgálat kerül ide
}

static void process_input (ami_t *ami) {
	// netsocket->inbuf hozzáfűzése az ami->inbuf stringhez egészen addig, amíg
	// az ami->inbuf -ban van hely. ami->inbuf_pos mutatja, hogy épp meddig terpeszkedik a string
	int freespace, bytes;
	char *pos; // ami->inbuf stringben az első "\r\n\r\n" lezáró token előtti pozíció
	int netsocket_offset = 0;

readnetsocket:
	freespace = sizeof(ami->inbuf) - ami->inbuf_pos - 1;
	bytes = (freespace < ami->netsocket->inbuf_len) ? freespace : ami->netsocket->inbuf_len;
	memmove(ami->inbuf + ami->inbuf_pos, ami->netsocket->inbuf + netsocket_offset, bytes);
	ami->inbuf_pos += bytes;

	if (
		!strcmp(ami->inbuf, "Asterisk Call Manager/1.1\r\n") ||
		!strcmp(ami->inbuf, "Asterisk Call Manager/1.0\r\n"))
	{
		ami->inbuf[0] = '\0';
		ami->inbuf_pos = 0;
		con_debug("Received \"Asterisk Call Manager\" header");
		netsocket_printf(ami->netsocket, "action: login\r\nusername: jsi\r\nsecret: pwd\r\n\r\n");
		return;
	}

checkdelim:
	if ((pos = strstr(ami->inbuf, "\r\n\r\n"))) {
		int offset = pos - ami->inbuf;

		parse_input(ami, ami->inbuf, offset + 2); // 2 = egy darab \r\n mérete

		// ha maradt még feldolgozandó adat, akkor azt a string elejére mozgatjuk
		if (ami->inbuf_pos > (offset + 4)) { // 4 = a "\r\n\r\n" lezaro merete
			memmove(ami->inbuf, ami->inbuf + offset + 4, ami->inbuf_pos - (offset + 4));
			ami->inbuf_pos -= (offset + 4);
			goto checkdelim;
		} else { // ha nincs már több adat, akkor string reset
			ami->inbuf[0] = '\0';
			ami->inbuf_pos = 0;
			return;
		}
	}

	// Az ami->inbuf -ban lévő szabad hely kiszámolása újra. Tehát arra vagyunk
	// kiváncsiak, hogy miután megpróbáltuk feldolgozni a csomagot, van -e még
	// szabad hely. Ha nincs, az nem jó! A gyakorlatban ez az eset akkor
	// következik be, ha az Asterisk az AMI_BUFSIZ makróban beállított buffer
	// méretnél több adatot küld \r\n\r\n lezáró nélkül.
	freespace = sizeof(ami->inbuf) - ami->inbuf_pos - 1;

	// Ha ide kerülünk, akkor gáz van, mert elfogyott a szabad hely, de még nincs elegendő
	// adat a bufferben ahhoz, hogy az üzenetet feldolgozzuk. Elképzelhető, hogy ezen
	// a ponton az egész netsocket kapcsolatot le kéne bontani, hogy a teljes folyamat
	// újrainduljon. Mert ha csak string reset van, akkor az a következő csomagnál
	// string nyesedéket eredményezhet, aminek megjósolhatatlan a kimenetele.
	// TODO: ezt az esetet netsocket_disconnect hívással kell lekezelni!
	if (!freespace) {
		con_debug("Buffer overflow, clearing ami->inbuf. ami->inbuf_pos=%d", ami->inbuf_pos);
		ami->inbuf[0] = '\0'; // string reset
		ami->inbuf_pos = 0;
		return;
	}

	//~ Ha ide jut a program, akkor az aktuális csomag fragmentálódott. Tehát,
	//~ amikor már van valami a bufferben, de még nem jött lezáró, azaz akkor,
	//~ amikor még nincs elegendő adat a csomag feldolgozásához. A töredék-csomag
	//~ a következő socket olvasásig a bufferben marad. Ez az eset a
	//~ gyakorlatban ritka, de különleges helyzetben néha előfordul. Ezen a
	//~ ponton nincs semmilyen műveletre, mert a függvény felépítéséből adódóan a
	//~ helyzet már le van kezelve.
	con_debug("fragmented packet from server");

	//~ Ha a függvény elején nem tudtuk átmásolni az összes netsocket->inbuf
	//~ bájtot az ami->inbuf bufferbe, akkor visszaugrunk a readnetsocket
	//~ címkéhez és ismét elkezdjük a netsocket->inbuf feldolgozását. Mivel az
	//~ ami->inbuf buffert időközben feldolgozta a parse_input(), ezért van benne
	//~ újra hely. A netsocket_offset változó gondoskodik arról, hogy a netsocket
	//~ ->inbuf buffert ne az elejétől másolja a memmove(), hanem onnan, ahol az
	//~ előbb félbemaradt.
	if (bytes < ami->netsocket->inbuf_len) {
		ami->netsocket->inbuf_len -= bytes;
		netsocket_offset += bytes;
		con_debug("goto readnetsocket");
		goto readnetsocket;
	}
}

static void netsocket_callback (netsocket_t *netsocket, int event) {
	ami_t *ami = netsocket->userdata;

	switch (event) {
		case NETSOCKET_EVENT_CONNECT:
			con_debug("Connected to %s (%s) port %d",
				netsocket->host,
				netsocket->ip,
				netsocket->port
			);
			break;

		case NETSOCKET_EVENT_DISCONNECT:
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

ami_t *ami_new (void *callback, void *userdata, struct ev_loop *loop) {
	ami_t *ami = malloc(sizeof(*ami));
	if (ami == NULL) {
		con_debug("ami_new() returned NULL");
		return NULL;
	}
	bzero(ami, sizeof(*ami)); // mindent nullázunk

	// ha meg van adva a loop paraméter, akkor azt használjuk eseménykezelőnek
	// ellenkező esetben az alapértelmezett eseménykezelőt
	ami->loop = (loop != NULL) ? loop : ev_default_loop(0);

	// default értékek
	strncpy(ami->host, AMI_DEFAULT_HOST, sizeof(ami->host) - 1);
	ami->port = AMI_DEFAULT_PORT;

	ami->callback = callback;
	ami->userdata = userdata;

	if (!(ami->netsocket = netsocket_new(netsocket_callback, ami, ami->loop))) {
		con_debug("netsocket_new returned NULL");
	}
	ami->netsocket->host = AMI_DEFAULT_HOST;
	ami->netsocket->port = AMI_DEFAULT_PORT;

	return ami;
}

void ami_destroy(ami_t *ami) {
	netsocket_destroy(ami->netsocket);
}

void ami_credentials (ami_t *ami, char *username, char *secret, char *host, char *port) {
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

ami_event_t *ami_action (ami_t *ami, void *callback, void *userdata, const char *fmt, ...) {
	ami_event_t *event = malloc(sizeof(ami_event_t));

}

ami_event_t *ami_event_register (ami_t *ami, void *callback, void *userdata, const char *fmt, ...) {
	ami_event_list_t *el = malloc(sizeof(ami_event_list_t));
	bzero(el, sizeof(el));

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(el->field_data, sizeof(el->field_data), fmt, ap);
	va_end(ap);

	tokenize_field(
		el->field,
		sizeof(el->field) / sizeof(char*) - 1,
		&el->field_size,
		el->field_data,
		sizeof(el->field_data)
	);

	// láncolt listába való illesztés kerül ide
}

int ami_event_unregister(ami_event_t *event) {

}

char *ami_getvar (ami_event_t *event, char *var) {
	int i;
	for (i = 0; i < event->field_size; i += 2) {
		if (!strcmp(event->field[i], var)) {
			if (event->field[i+1] != NULL) {
				return event->field[i+1];
			} else {
				return ""; // TODO: jó ez? Nem NULL kéne ide is?
			}
		}
	}
	return NULL;
}

void ami_strncpy (ami_event_t *event, char *dest, char *var, size_t maxsize) {

}

