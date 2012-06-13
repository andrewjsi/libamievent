#include <stdlib.h>
#include <unistd.h>

#include "ami.h"

int ami_init (ami_t *ami, char *host, int port, char *username, char *secret, void *callback) {
	return 0;
}

void ami_destroy(ami_t *ami) {

}

void ami_connect (ami_t *ami) {

}

ami_event_t ami_action (ami_t *ami, void *callback, void *userdata, const char *fmt, ...) {
	ami_event_t *event = malloc(sizeof(ami_event_t));

}

ami_event_t *ami_event_register (ami_t *ami, void *callback, void *userdata, const char *fmt, ...) {
	ami_event_t *event = malloc(sizeof(ami_event_t));

}

int ami_event_unregister(ami_event_t *event) {

}

