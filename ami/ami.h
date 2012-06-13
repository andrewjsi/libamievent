#include "netsocket.h"

#ifndef AMI_DEFAULT_HOST
#define AMI_DEFAULT_HOST "localhost"
#endif

#ifndef AMI_DEFAULT_PORT
#define AMI_DEFAULT_PORT 5038
#endif

typedef struct ami_t {
	char host[64];
	int port;
	char username[32];
	char secret[32];
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

ami_t *ami_new (void *callback, void *userdata);
void ami_credentials (ami_t *ami, char *username, char *secret, char *host, char *port);
void ami_destroy(ami_t *ami);
void ami_connect (ami_t *ami);
ami_event_t *ami_action (ami_t *ami, void *callback, void *userdata, const char *fmt, ...);
ami_event_t *ami_event_register (ami_t *ami, void *callback, void *userdata, const char *fmt, ...);
int ami_event_unregister(ami_event_t *event);
char *ami_getvar (ami_event_t *event, char *var);
void ami_strncpy (ami_event_t *event, char *dest, char *var, size_t maxsize);


