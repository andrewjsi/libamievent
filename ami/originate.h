#ifndef ORIGINATE_H_INCLUDED
#define ORIGINATE_H_INCLUDED

#include "ami.h"

typedef struct ami_ori_t {
	ami_t *ami;
	char uuid[16];
	ami_event_list_t *action_originate;
	ami_event_list_t *event_originateresponse;
	ami_event_list_t *event_varsetuuid;
} ami_ori_t;

ami_ori_t *ami_originate (ami_t *ami, const char *fmt, ...);

void ami_originate_free (ami_ori_t *ami_ori);

#endif // #ifndef ORIGINATE_H_INCLUDED
