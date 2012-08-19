#include "netsocket.h"

#ifndef AMI_DEFAULT_HOST
#define AMI_DEFAULT_HOST "localhost"
#endif

#ifndef AMI_DEFAULT_PORT
#define AMI_DEFAULT_PORT 5038
#endif

#ifndef AMI_BUFSIZ
#define AMI_BUFSIZ 4096
#endif

#ifndef AMI_FIELD_SIZE
#define AMI_FIELD_SIZE 64
#endif

enum ami_event_type {
	AMI_EVENT = 1,
	AMI_RESPONSE,
	AMI_CLIRESPONSE,
	AMI_CONNECT,
	AMI_DISCONNECT,
};

// ha változik, akkor egyeztess az ami.c ami_dump_lists() függvénnyel!
typedef struct ami_event_list_t {
    struct ami_event_list_t *prev;
    struct ami_event_list_t *next;
	void (*callback)(void*);
	void *userdata;
	int field[AMI_FIELD_SIZE];
	int field_size;
	char data[AMI_BUFSIZ];
	char *regby_file;
	int regby_line;
	const char *regby_function;
	unsigned int action_id;
	enum ami_event_type type;
} ami_event_list_t;

typedef struct ami_event_t {
    struct ami_event_t *prev;
    struct ami_event_t *next;
	struct ami_t *ami;
	int success; // csak "Response: Success" esetén lesz egy, tehát biztos hogy volt Response és az értéke Success volt
	int field[AMI_FIELD_SIZE];
	int field_size;
	char data[AMI_BUFSIZ];
	int data_size;
	void (*callback)(void*);
	void *userdata;
	unsigned int action_id;
	char *regby_file;
	int regby_line;
	const char *regby_function;
	enum ami_event_type type;
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
	struct ev_loop *loop;                       // eseményhurok
	ev_timer need_event_processing;				// azonnali idõzítõ az események kiküldéséhez
	char inbuf[AMI_BUFSIZ];                     // bejövõ buffer
	int inbuf_pos;                              // bejövõ buffer pozíciója
	struct ami_event_t *event_head;             // esemény várakozósor
	struct ami_event_t event_tmp;				// itt készítünk új eseményt, amit aztán a várakozósorba másolunk
	int authenticated;                          // 1 lesz sikeres login után
	unsigned int action_id;						// soron következõ használható ActionID
} ami_t;

ami_t *ami_new (void *callback, void *userdata, struct ev_loop *loop);

void ami_credentials (ami_t *ami, char *username, char *secret, char *host, char *port);

void ami_destroy(ami_t *ami);

void ami_connect (ami_t *ami);

int ami_printf (ami_t *ami, const char *fmt, ...);

#define ami_action(ami,callback,userdata,...) _ami_action(ami, callback, userdata, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
ami_event_list_t *_ami_action (ami_t *ami, void *callback, void *userdata, char *file, int line, const char *function, const char *fmt, ...);

#define ami_event_register(ami,callback,userdata,...) _ami_event_register(ami, callback, userdata, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
ami_event_list_t *_ami_event_register (ami_t *ami, void *callback, void *userdata, char *file, int line, const char *function, const char *fmt, ...);

int ami_event_unregister(ami_event_list_t *el);

char *ami_getvar (ami_event_t *event, char *var);

#define ami_strcpy(event,dest,var) ami_strncpy(event,dest,var,sizeof(dest))

void ami_strncpy (ami_event_t *event, char *dest, char *var, size_t maxsize);

void ami_dump_lists (ami_t *ami);
