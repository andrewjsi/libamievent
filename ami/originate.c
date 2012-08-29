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
static void generate_uuid (char *dst, size_t size) {
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

static void got_uniqueid (ami_ori_t *ami_ori, char *uniqueid) {
	ami_t *ami = ami_ori->ami;
	//~ strncpy(ami_ori->uniqueid, uniqueid, sizeof(ami_ori->uniqueid) - 1);
	ami_event_register(ami, dumpi, NULL, "Uniqueid: %s", uniqueid);
}

static void event_gotuuid_cb (ami_event_t *event) {
	ami_ori_t *ami_ori = (ami_ori_t*)event->userdata;
	char *uniqueid = ami_getvar(event, "Uniqueid");

	printf("Megvan az uniqueid ==== %s\n", uniqueid);
	got_uniqueid(ami_ori, uniqueid);
}

static void response_originate (ami_event_t *event) {
	ami_ori_t *ami_ori = (ami_ori_t*)event->userdata;
	ami_t *ami = ami_ori->ami;

	ami_event_dump(event);

	char *uniqueid = ami_getvar(event, "Uniqueid");
printf("*** ORIGINATE RESPONSE\n");
	if (strlen(uniqueid)) {
		printf("RESPONSE szinten megvan az uniqueid ==== %s\n", uniqueid);
		got_uniqueid(ami_ori, uniqueid);
		ami_event_unregister(ami, ami_ori->event_varsetuuid);
		ami_ori->event_varsetuuid = NULL;
	}
}

// megrendelt AMI eventek lemondása
void cleanup_events (ami_ori_t *ami_ori) {
	ami_t *ami = ami_ori->ami;

	ami_event_unregister(ami, ami_ori->event_originateresponse);
	ami_ori->event_originateresponse = NULL;

	ami_event_unregister(ami, ami_ori->event_varsetuuid);
	ami_ori->event_varsetuuid = NULL;

	ami_event_unregister(ami, ami_ori->action_originate);
	ami_ori->action_originate = NULL;
}

ami_ori_t *ami_originate (ami_t *ami, const char *fmt, ...) {
	ami_ori_t *ami_ori = malloc(sizeof(*ami_ori));
	if (ami_ori == NULL) {
		con_debug("ami_originate() returned NULL");
		return NULL;
	}
	bzero(ami_ori, sizeof(*ami_ori)); // minden NULL

	ami_ori->ami = ami;

	char buf[AMI_BUFSIZ];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	buf[AMI_BUFSIZ-1] = '\0'; // védelem

	// AMI UUID
	generate_uuid(ami_ori->uuid, sizeof(ami_ori->uuid));

	ami_ori->event_varsetuuid = ami_event_register(ami_ori->ami, event_gotuuid_cb, ami_ori,
		"Event: VarSet\n"
		"Variable: ami_originate_id\n"
		"Value: %s\n"
		, ami_ori->uuid
	);

	con_debug("sending originate with id %s", ami_ori->uuid);
	ami_ori->action_originate = ami_action(ami, response_originate, ami_ori,
		"Action: Originate\n"
		"Async: 1\n"
		"Variable: ami_originate_id=%s\n"
		"%s"
		, ami_ori->uuid, buf
	);

	return ami_ori;
}

void ami_originate_destroy (ami_ori_t *ami_ori) {

}

