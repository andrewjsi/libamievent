#include <stdlib.h>
#include <unistd.h>

#include "ami.h"

ami_t *ami_init (char *host, int port, char *username, char *secret, void *callback) {
	return 1;
}

void ami_destroy(ami_t *ami) {

}

void ami_connect (ami_t *ami) {

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



