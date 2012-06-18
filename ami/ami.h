#include "netsocket.h"

#ifndef AMI_DEFAULT_HOST
#define AMI_DEFAULT_HOST "localhost"
#endif

#ifndef AMI_DEFAULT_PORT
#define AMI_DEFAULT_PORT 5038
#endif

#ifndef AMI_BUFSIZ
#define AMI_BUFSIZ 2048
#endif

// ha változik, akkor egyeztess az ami.c ami_dump_lists() függvénnyel!
typedef struct ami_event_list_t {
    struct ami_event_list_t *prev;
    struct ami_event_list_t *next;
	void (*callback)(void*);
	void *userdata;
	char *field[64];
	int field_size;
	char field_data[512];
	char *stack_file;
	int stack_line;
	const char *stack_function;
} ami_event_list_t;

typedef struct ami_action_list_t {
	void (*callback)(void*);
	void *userdata;
    struct ami_action_list_t *prev;
    struct ami_action_list_t *next;
} ami_action_list_t;

typedef struct ami_event_t {
	struct ami_t *ami;
	int err; // Response esetén 0=SUCCESS 1=minden más
	//~ char actionid[16]; // Action esetén ide kerül az ActionID
	char *field[64];
	int field_size;
	void (*callback)(void*);
	void *userdata;
	ami_event_list_t *invokedby;
	enum {
		AMI_EVENT = 1,
		AMI_RESPONSE,
		AMI_CLIRESPONSE,
		AMI_CONNECT,
		AMI_DISCONNECT,
	} type;
} ami_event_t;

typedef struct ami_t {
	char host[64];                              // Asterisk host
	int port;                                   // Asterisk Manager Interface port
	char username[32];                          // AMI User
	char secret[32];                            // AMI Password
	netsocket_t *netsocket;                     // Netsocket objektum
	char disconnect_reason[64];                 // ???
	void (*callback)(void*);                    // Callback
	void *userdata;                             // Callback-nek átadott általános mutató
	ami_event_list_t *ami_event_list_head;      // megrendelt események
	ami_action_list_t *ami_action_list_head;    // kiküldött parancs tárolása a visszajövõ Response üzenethez
	struct ev_loop *loop;                       // eseményhurok
	char inbuf[AMI_BUFSIZ];                     // bejövõ buffer
	int inbuf_pos;                              // bejövõ buffer pozíciója
	struct ami_event_t event;
} ami_t;

ami_t *ami_new (void *callback, void *userdata, struct ev_loop *loop);

void ami_credentials (ami_t *ami, char *username, char *secret, char *host, char *port);

void ami_destroy(ami_t *ami);

void ami_connect (ami_t *ami);

ami_event_t *ami_action (ami_t *ami, void *callback, void *userdata, const char *fmt, ...);

#define ami_event_register(ami,callback,userdata,...) _ami_event_register(ami, callback, userdata, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

//~ ami_event_t *_ami_event_register (ami_t *ami, void *callback, void *userdata, char *file, char *line, char *function, const char *fmt, ...);
ami_event_t *_ami_event_register (ami_t *ami, void *callback, void *userdata, char *file, int line, const char *function, const char *fmt, ...);

int ami_event_unregister(ami_event_t *event);

char *ami_getvar (ami_event_t *event, char *var);

void ami_strncpy (ami_event_t *event, char *dest, char *var, size_t maxsize);

void ami_dump_lists (ami_t *ami);
