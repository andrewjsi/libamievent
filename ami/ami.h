
typedef struct ami_t {
	char *host;
	int port;
	void (*callback)(void*);
	void *userdata;
} ami_t;

typedef struct ami_event_t {
	char **vars;
	void (*callback)(void*);
	void *userdata;
} ami_event_t;

typedef struct ami_action_t {
	int err;
	char **vars;
	void (*callback)(void*);
	void *userdata;
} ami_action_t;

