
typedef struct ami_t {
	char *host;
	int port;
	void (*callback)(void*);
	void *userdata;
} ami_t;

typedef struct ami_event_t {
	int err;							// Response esetén 0=SUCCESS 1=minden más
	char actionid[16];					// Action esetén ide kerül az ActionID, Respo
	ami_event_list
	char **vars;
	void (*callback)(void*);
	void *userdata;
	typedef enum type {
		AMI_EVENT = 1,
		AMI_RESPONSE,
		AMI_CLIRESPONSE,
		AMI_CONNECT,
		AMI_DISCONNECT,
	} type;
} ami_event_t;

