//~ TODO: a process_input() nem teljesen korrekt. Ha több csomag érkezik
//~ egyszerre, akkor mintha nem mindegyik érkezne be... Megvizsgálni!

//~ TODO: 1.4-es Asteriskre csatlakozva baszik menni. 1.8 OK.

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ev.h>
#include <stdio.h> // TODO: kell ez?

#include "ami.h"
#include "debug.h"

#define CON_DEBUG
#include "logger.h"

// teszt kedvéért
static void parse_input (ami_t *ami, char *buf, int size) {
	int i;
	for (i = 0; i < size; i++) {
		putchar(buf[i]);
	}
	printf("\n");
}

static void process_input (ami_t *ami) {
	// netsocket->inbuf hozzáfűzése az ami->inbuf stringhez egészen addig, amíg
	// az ami->inbuf -ban van hely. ami->inbuf_pos mutatja, hogy épp meddig terpeszkedik a string
	int freespace, bytes;
	char *pos; // ami->inbuf stringben az első "\r\n\r\n" lezáró token előtti pozíció

	freespace = sizeof(ami->inbuf) - ami->inbuf_pos - 1;
	bytes = (freespace < ami->netsocket->inbuf_len) ? freespace : ami->netsocket->inbuf_len;
	memmove(ami->inbuf + ami->inbuf_pos, ami->netsocket->inbuf, bytes);
	ami->inbuf_pos += bytes;

	if (
		!strcmp(ami->inbuf, "Asterisk Call Manager/1.1\r\n") ||
		!strcmp(ami->inbuf, "Asterisk Call Manager/1.0\r\n"))
	{
		ami->inbuf[0] = '\0';
		ami->inbuf_pos = 0;
		con_debug("Received \"Asterisk Call Manager\" header");
		netsocket_printf(ami->netsocket, "action: login\nusername: jsi\nsecret: pwd\n\n");
		return;
	}

start:
	if ((pos = strstr(ami->inbuf, "\r\n\r\n"))) {
		int offset = pos - ami->inbuf;
		//~ debi(pos); debi(ami->inbuf); debi(ami->inbuf_pos); debi(offset);

		parse_input(ami, ami->inbuf, offset + 2); // 2 = egy darab \r\n mérete

		// ha maradt még feldolgozandó adat, akkor azt a string elejére mozgatjuk
		if (ami->inbuf_pos > (offset + 4)) { // 4 = a "\r\n\r\n" lezaro merete
			memmove(ami->inbuf, ami->inbuf + offset + 4, ami->inbuf_pos - (offset + 4));
			ami->inbuf_pos = 0;
			goto start;
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
	ami_event_t *event = malloc(sizeof(ami_event_t));

}

int ami_event_unregister(ami_event_t *event) {

}

char *ami_getvar (ami_event_t *event, char *var) {

}

void ami_strncpy (ami_event_t *event, char *dest, char *var, size_t maxsize) {

}

