#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ami.h"

ami_t *ami_new (void *callback, void *userdata) {
	ami_t *ami = malloc(sizeof(*ami));
	bzero(ami, sizeof(*ami)); // mindent nullázunk

	// default értékek
	strncpy(ami->host, AMI_DEFAULT_HOST, sizeof(ami->host) - 1);
	ami->port = AMI_DEFAULT_PORT;

	ami->callback = callback;
	ami->userdata = userdata;

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

void ami_destroy(ami_t *ami) {

}

void ami_connect (ami_t *ami) {
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



