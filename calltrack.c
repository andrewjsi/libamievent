#include <stdio.h>
#include <string.h>
#include <ev.h>
#include <stdlib.h>
#include <unistd.h>

#include "ami.h"
#include "originate.h"
#include "debug.h"

#define CON_DEBUG
#include "logger.h"

ami_t *ami;
char dialstring[64];

void originate_event (ori_t *ori, ami_event_t *event) {
	switch (ori->state) {
		case ORI_DIALING:
			printf("Tárcsázás...\n");
			break;

		case ORI_RINGING:
			printf("Kicsöng\n");
			break;

		case ORI_ANSWERED:
			printf("Felvette\n");
			break;

		case ORI_HANGUP:
			printf("Hívás bontva (%d) (%s)\n", ori->hangupcause, ori->hangupcausetxt);
			exit(0);
			break;

		case ORI_UNKNOWN:
			printf("Ismeretlen ori->state !!!\n");
			break;

	}

}

// Event: Connect
void event_connect (ami_event_t *event) {
	printf("Connected to %s:%s\n",
		ami_getvar(event, "Host"),
		ami_getvar(event, "Port")
	);

	printf("Calling %s\n", dialstring);
	ami_originate(event->ami, originate_event, NULL,
		"Channel: %s\n"
		"Context: default\n"
		"Exten: 42\n"
		"Priority: 1\n"
		, dialstring
	);

}

// Event: Disconnect
void event_disconnect (ami_event_t *event) {
	sleep(1); // TODO: ezt a blokkolást ami_connect_delayed() hívással kezelni!
	ami_connect(event->ami);
}

//~ void event_originateresponse (ami_event_t *event) {
	//~ printf("OriginateResponse! channel=%s uniqueid=%s\n",
		//~ ami_getvar(event, "Channel"),
		//~ ami_getvar(event, "Uniqueid")
	//~ );
//~ }

int main (int argc, char *argv[]) {
	ami = ami_new(EV_DEFAULT);
	if (ami == NULL) {
		con_debug("ami_new() returned NULL");
		return 1;
	}

	char host[128];
	if (argc > 1)
		strncpy(host, argv[1], sizeof(host));
	else
		strcpy(host, "192.168.15.200");

	if (argc > 2)
		strncpy(dialstring, argv[2], sizeof(dialstring));
	else
		strcpy(dialstring, "DAHDI/g1/2480999");

	ami_credentials(ami, "jsi", "pwd", host, "5038");
	ami_connect(ami);

	ami_event_register(ami, event_disconnect, NULL, "Disconnect");
	ami_event_register(ami, event_connect, NULL, "Connect");
	//~ ami_event_register(ami, event_originateresponse, NULL, "Event: OriginateResponse");


	//~ printf("\n");
	//~ ami_dump_lists(ami);
con_debug("Entering the main loop");
	ev_loop(EV_DEFAULT, 0);

	ami_destroy(ami);
	return 0;
}



