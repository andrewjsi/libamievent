#ifndef ORIGINATE_H_INCLUDED
#define ORIGINATE_H_INCLUDED

#include "ami.h"

typedef struct ori_t {
    ami_t *ami;
    char oid[16];                                 // ori objektum saját egyedi azonosítója VarSet-hez
    char uniqueid[48];                            // Asterisk híváshoz rendelt Uniqueid
    char channel[48];
    char local_slave_channel[48];                 // Asterisk 1.4 esetén Local csatorna másik neve
    void (*callback)(void*,ami_event_t*);
    void *userdata;
    int hangupcause;                              // ide került a bontás kódja
    char hangupcausetxt[64];                      // ide kerül a bontás szövege
    ami_event_t *ami_event;
    ami_event_list_t *action_originate;
    ami_event_list_t *event_varset_uuid;
    ami_event_list_t *event_newstate;
    ami_event_list_t *event_newcallerid;           // Asterisk 1.4-nél innen tudjuk meg a csatornát
    ami_event_list_t *event_hangup;
    ami_event_list_t *event_hangup2;            // Asterisk 1.4 hangup cause-code érzékelés
    ami_event_list_t *event_originateresponse_failure; // itt figyeljük, ha az Originate kudarcba fullad
    enum {
        ASTERISK14,
        ASTERISK16,
    } asterisk_version;
    enum {
        ORI_UNKNOWN = 0,
        ORI_ANSWERED = 1,
        ORI_HANGUP,
        ORI_DIALING,
        ORI_RINGING,
    } state;                                      // esemény típusa, hívás állapota, TODO: megfogalmazni rendesen
} ori_t;

ori_t *ami_originate (ami_t *ami, void *callback, void *userdata, const char *fmt, ...);

void ami_originate_free (ori_t *ori);

#endif // #ifndef ORIGINATE_H_INCLUDED
