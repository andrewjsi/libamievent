#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ev.h>

#include "ami.h"
#include "debug.h"

#define CON_DEBUG
#include "logger.h"

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
			//~ process_input();
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



