
typedef struct ami_t {
	char *host;
	int port;
	void (*callback)(void*);
	void *userdata;
} ami_t;

typedef struct ami_event_t {
	int err;							// Response eset�n 0=SUCCESS 1=minden m�s
	char actionid[16];					// Action eset�n ide ker�l az ActionID, Respo
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

