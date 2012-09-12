#define CON_DEBUG

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

static void dumpi (ami_event_t *event) {
	ami_event_dump(event);
}

static void got_uniqueid (ori_t *ori, char *uniqueid) {
	ami_t *ami = ori->ami;
	//~ strncpy(ori->uniqueid, uniqueid, sizeof(ori->uniqueid) - 1);
	ami_event_register(ami, dumpi, NULL, "Uniqueid: %s", uniqueid);
}

static void event_gotuuid_cb (ami_event_t *event) {
	ori_t *ori = (ori_t*)event->userdata;
	char *uniqueid = ami_getvar(event, "Uniqueid");

	con_debug("got UniqueID=%s via VarSet method", uniqueid);
	got_uniqueid(ori, uniqueid);
}

static void response_originate (ami_event_t *event) {
	ori_t *ori = (ori_t*)event->userdata;
	ami_t *ami = ori->ami;

	ami_event_dump(event);

	char *uniqueid = ami_getvar(event, "Uniqueid");
	if (strlen(uniqueid)) {
		con_debug("got UniqueID=%s via Originate Response method", uniqueid);
		got_uniqueid(ori, uniqueid);
		ami_event_unregister(ami, ori->event_varset_uuid);
		ori->event_varset_uuid = NULL;
	}
}

// megrendelt AMI eventek lemondása
void cleanup_events (ori_t *ori) {
	ami_t *ami = ori->ami;

	ami_event_unregister(ami, ori->event_varset_uuid);
	ori->event_varset_uuid = NULL;

	ami_event_unregister(ami, ori->action_originate);
	ori->action_originate = NULL;
}

ori_t *ami_originate (ami_t *ami, const char *fmt, ...) {
	ori_t *ori = malloc(sizeof(*ori));
	if (ori == NULL) {
		con_debug("ami_originate() returned NULL");
		return NULL;
	}
	bzero(ori, sizeof(*ori)); // minden NULL

	ori->ami = ami;

	char buf[AMI_BUFSIZ];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	buf[AMI_BUFSIZ-1] = '\0'; // védelem

	// AMI UUID
	generate_oid(ori->oid, sizeof(ori->oid));

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

	return ori;
}

void ami_originate_destroy (ori_t *ori) {

}

