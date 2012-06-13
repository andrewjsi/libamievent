#include "netsocket.h"

typedef struct ami_t {
	char *host;
	int port;
	netsocket_t *netsocket;
	char disconnect_reason[64];
	void (*callback)(void*);
	void *userdata;
} ami_t;

typedef struct ami_event_t {
	ami_t *ami;
	int err;							// Response esetén 0=SUCCESS 1=minden más
	char actionid[16];					// Action esetén ide kerül az ActionID
	char **vars;
	void (*callback)(void*);
	void *userdata;
	enum {
		AMI_EVENT = 1,
		AMI_RESPONSE,
		AMI_CLIRESPONSE,
		AMI_CONNECT,
		AMI_DISCONNECT,
	} type;

} ami_event_t;


ami_t *ami_init (char *host, int port, char *username, char *secret, void *callback);
void ami_destroy(ami_t *ami);
void ami_connect (ami_t *ami);
ami_event_t *ami_action (ami_t *ami, void *callback, void *userdata, const char *fmt, ...);
ami_event_t *ami_event_register (ami_t *ami, void *callback, void *userdata, const char *fmt, ...);
int ami_event_unregister(ami_event_t *event);
char *ami_getvar (ami_event_t *event, char *var);
void ami_strncpy (ami_event_t *event, char *dest, char *var, size_t maxsize);


