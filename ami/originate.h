#ifndef ORIGINATE_H_INCLUDED
#define ORIGINATE_H_INCLUDED

#include "ami.h"

typedef struct ori_t {
	ami_t *ami;
	char oid[16];                                 // ori objektum saját egyedi azonosítója VarSet-hez
	char uniqueid[32];                            // Asterisk híváshoz rendelt Uniqueid
	ami_event_list_t *action_originate;
	ami_event_list_t *event_varset_uuid;
} ori_t;

ori_t *ami_originate (ami_t *ami, const char *fmt, ...);

void ami_originate_free (ori_t *ori);

#endif // #ifndef ORIGINATE_H_INCLUDED
