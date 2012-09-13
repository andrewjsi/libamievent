// originate.c-hez tartozó con_debug() üzenetek beforduljanak -e
//~ #define CON_DEBUG

// ami_event_dump() futtatása az originate eseményekre (kommentezd ha nem kell!)
//~ #define AMI_ORIGINATE_DUMP_EVENTS

// TODO: kigyomlálni a nem kellő include-okat
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ev.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h> // gettimeofday()

#include "originate.h"
#include "debug.h"
#include "utlist.h"
#include "misc.h"
#include "logger.h"

/* Működés:

Asterisk 1.4 esetén
1. > Action Originate
2. < a visszajövő Response tartalmazza az UniqueID-t
3. < Event Newcallerid tartalmazza a Channel-t

Asterisk 1.6 és magasabb esetén
1. > Action Originate, benne egy Variable: ami_originate_id = ori->oid
2. < a visszajövő Response nem tartalmaz érdemi információt
3. < Event Varset tartalmazza a fent megadott ami_origate_id-t, Channelt- és UniqueID-t

A továbbiakban Newstate esemény mutatja a hívás állapotát és a Hangup pedig
a bontás okát, illetve az "Event: OriginateResponse\nResponse: Failure"
jelzi az Originate azonnali kudarcát.

*/

// 6 byte-os random hexa stringet masol az ami->uuid bufferbe
// TODO: egy rendes, unique ID-t visszaado fuggvenyt irni ehelyett a random vacak helyett
static void generate_oid (char *dst, size_t size) {
	struct timeval tv;
	int num;
	char tmp[16];

	gettimeofday(&tv, NULL);
	srand(tv.tv_usec * tv.tv_sec);
	num = rand();
	snprintf(tmp, sizeof(tmp), "%x", num);
	tmp[6] = '\0';
	strncpy(dst, tmp, size);
}

static void invoke_callback (ori_t *ori, ami_event_t *event) {
	ori->callback(ori, event);
}

static void got_ami_event (ami_event_t *event) {
	ori_t *ori = (ori_t*)event->userdata;

#ifdef AMI_ORIGINATE_DUMP_EVENTS
	ami_event_dump(event);
#endif

	ori->state = UNKNOWN;
	ori->hangupcause = 0;
	ori->hangupcausetxt[0] = '\0';

	char *state_var_name = (ori->asterisk_version == ASTERISK14) ? "State" : "ChannelStateDesc";
	con_debug("requested variable name in Newstate event is %s", state_var_name);

	if (!strcmp(ami_getvar(event, "Event"), "Newstate")) {
		if (!strcmp(ami_getvar(event, state_var_name), "Dialing")) {
			ori->state = DIALING;
		} else if (!strcmp(ami_getvar(event, state_var_name), "Ringing")) {
			ori->state = RINGING;
		} else if (!strcmp(ami_getvar(event, state_var_name), "Up")) {
			ori->state = ANSWERED;
		} else if (!strcmp(ami_getvar(event, state_var_name), "Down")) {
			con_debug("ignoring Event=Newstate %s=Down", state_var_name);
			return;
		}
	} else if (!strcmp(ami_getvar(event, "Event"), "Hangup")) {
		ori->state = HANGUP;
		ori->hangupcause = atoi(ami_getvar(event, "Cause"));
		strncpy(ori->hangupcausetxt, ami_getvar(event, "Cause-txt"), sizeof(ori->hangupcausetxt));
	}

	invoke_callback(ori, event);
}

static void got_uniqueid_and_channel (ori_t *ori, char *uniqueid, char *channel) {
	ami_t *ami = ori->ami;
	if (uniqueid) {
		strncpy(ori->uniqueid, uniqueid, sizeof(ori->uniqueid));
		ori->event_hangup = ami_event_register(
			ami, got_ami_event, ori,
			"Event: Hangup\nUniqueid: %s",
			uniqueid
		);
	}
	if (channel) {
		/* Ha megvan a channel, akkor már nem érdekes a Newcallerid */
		if (ori->event_newcallerid) {
			ami_event_unregister(ami, ori->event_newcallerid);
			ori->event_newcallerid = NULL;
		}

		/* Ha van channel, akkor már nem kell az "Event:
		OriginateResponse\nResponse: Failure", mert ha a hívás nem is
		sikerül, attól még biztosan lesz Hangup. Azért mondjuk le az
		OriginateResponse eseményt, mert ha Ringing közben történik a
		Hangup, akkor az OriginateResponse Failure is megtörténik, ami akkor
		már nem érdekes. Más szavakkal mondva: ha eddig a pontig nem
		érkezett OriginateResponse Failure, akkor a továbbiakban nem is
		veszünk róla tudomást, hisz a Hangup értesít minket a hívás
		sikertelenségéről vagy a befejezéséről */
		if (ori->event_originateresponse_failure) {
			ami_event_unregister(ami, ori->event_originateresponse_failure);
			ori->event_originateresponse_failure = NULL;
		}
		strncpy(ori->channel, channel, sizeof(ori->channel));
		ori->event_newstate = ami_event_register(
			ami, got_ami_event, ori,
			"Event: Newstate\nChannel: %s",
			channel
		);
	}
}

static void event_gotuuid_cb (ami_event_t *event) {
	ori_t *ori = (ori_t*)event->userdata;
	char *uniqueid = ami_getvar(event, "Uniqueid");
	char *channel = ami_getvar(event, "Channel");

	con_debug("got UniqueID=%s, Channel=%s via VarSet method", uniqueid, channel);
	got_uniqueid_and_channel(ori, uniqueid, channel);
}

static void event_newcallerid_cb (ami_event_t *event) {
	ori_t *ori = (ori_t*)event->userdata;

	char *channel = ami_getvar(event, "Channel");
	con_debug("got Channel=%s via Newcallerid method", channel);
	got_uniqueid_and_channel(ori, NULL, channel);
}

static void response_originate (ami_event_t *event) {
	ori_t *ori = (ori_t*)event->userdata;
	ami_t *ami = ori->ami;

#ifdef AMI_ORIGINATE_DUMP_EVENTS
	ami_event_dump(event);
#endif

	char *uniqueid = ami_getvar(event, "Uniqueid");
	// Ha itt szerepel az UniqueID, akkor Asterisk 1.4-ről van szó
	if (strlen(uniqueid)) {
		con_debug("got UniqueID=%s via Originate Response method", uniqueid);
		ori->asterisk_version = ASTERISK14;
		got_uniqueid_and_channel(ori, uniqueid, NULL);
		ami_event_unregister(ami, ori->event_varset_uuid);
		ori->event_varset_uuid = NULL;
		ori->event_newcallerid = ami_event_register(ami, event_newcallerid_cb, ori,
		                                            "Event: Newcallerid\nUniqueid: %s",
		                                            uniqueid);
	}
}

static void event_originateresponse_failure_cb (ami_event_t *event) {
	ori_t *ori = (ori_t*)event->userdata;

	ori->state = HANGUP;
	ori->hangupcause = 0;
	strncpy(ori->hangupcausetxt, "Origination failure", sizeof(ori->hangupcausetxt));
	invoke_callback(ori, NULL);

	char *reason = ami_getvar(event, "Reason");
	con_debug("Action Originate failed, Reason=%s", reason);
}

// megrendelt AMI eventek lemondása
// TODO: az összeset felvenni ide!
void cleanup_events (ori_t *ori) {
	ami_t *ami = ori->ami;

	ami_event_unregister(ami, ori->event_varset_uuid);
	ori->event_varset_uuid = NULL;

	ami_event_unregister(ami, ori->action_originate);
	ori->action_originate = NULL;
}

ori_t *ami_originate (ami_t *ami, void *callback, void *userdata, const char *fmt, ...) {
	ori_t *ori = malloc(sizeof(*ori));
	if (ori == NULL) {
		con_debug("ami_originate() returned NULL");
		return NULL;
	}
	bzero(ori, sizeof(*ori)); // minden NULL

	ori->ami = ami;
	ori->asterisk_version = ASTERISK16;

	char buf[AMI_BUFSIZ];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	buf[AMI_BUFSIZ-1] = '\0'; // védelem

	// ami_originate_id a Varset módszerhez
	generate_oid(ori->oid, sizeof(ori->oid));

	ori->callback = callback;
	ori->userdata = userdata;

	ori->event_varset_uuid = ami_event_register(ori->ami, event_gotuuid_cb, ori,
		"Event: VarSet\n"
		"Variable: ami_originate_id\n"
		"Value: %s\n"
		, ori->oid
	);

	con_debug("sending originate with id %s", ori->oid);
	ori->action_originate = ami_action(ami, response_originate, ori,
		"Action: Originate\n"
		"Async: 1\n"
		"Variable: ami_originate_id=%s\n"
		"%s"
		, ori->oid, buf
	);

	/* Az "Event: OriginateResponse" egy speciális Asterisk esemény. Talán
	ez az egyetlen olyan esemény, ami konkrétan egy Action hatására jön létre
	és rendelkezik ActionID-vel. Ezt az eseményt az "Action:
	Originate\nAsync: 1" okozza akkor, amikor az originate során az első
	csatorna csatlakozik vagy hibába esik. Ez az esemény hordozza a Channel
	és UniqueID változókat, de nem innen vesszük őket, mert mire ez az
	esemény megérkezik, addigra már más események (pl. Ringing) rég
	megtörténtek. Viszont ha az Originate azonnal kudarcba fullad (pl. nem
	létező Channel Technology megadása esetén), akkor erről a tényről csak
	ezen a ponton tudunk értesülni. */
	ori->event_originateresponse_failure = ami_event_register(
		ami, event_originateresponse_failure_cb, ori,
		"Event: OriginateResponse\nResponse: Failure\nActionID: %d",
		ori->action_originate->action_id
	);

	return ori;
}

void ami_originate_destroy (ori_t *ori) {
	// TODO: megírni:)
}

