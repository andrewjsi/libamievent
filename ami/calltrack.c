#include <stdio.h>
#include <string.h>
#include <ev.h>
#include <stdlib.h>

#include "ami.h"
#include "debug.h"

//~ #define CON_DEBUG
#include "logger.h"

ami_t *ami;

// Event: Connect
void event_connect (ami_event_t *event) {
	printf("Connected to %s:%s\n",
		ami_getvar(event, "Host"),
		ami_getvar(event, "Port"));
}

// Event: Disconnect
void event_disconnect (ami_event_t *event) {
	ami_connect(event->ami);
}

int main (int argc, char *argv[]) {
	ami = ami_new(EV_DEFAULT);
	if (ami == NULL) {
		con_debug("ami_new() returned NULL");
		return 1;
	}

	char host[128];
	if (argc < 2)
		strcpy(host, "10.27.1.222");
	else
		strcpy(host, argv[1]);

	ami_credentials(ami, "jsi", "pwd", host, "5038");
	ami_connect(ami);

	ami_event_register(ami, event_disconnect, NULL, "Disconnect");
	ami_event_register(ami, event_connect, NULL, "Connect");

	printf("\n");
	ami_dump_lists(ami);

	ev_loop(EV_DEFAULT, 0);

	ami_destroy(ami);
	return 0;
}



