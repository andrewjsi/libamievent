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

static void event_gotuuid_cb (ami_event_t *event) {
	//~ ami_ori_t *ami_ori = (ami_ori_t*)event->userdata;
	char *uniqueid = ami_getvar(event, "Uniqueid");

	printf("Megvan az uniqueid ==== %s\n", uniqueid);
}

static void response_originate (ami_event_t *event) {

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

	ami_ori->event_gotuuid = ami_event_register(ami_ori->ami, event_gotuuid_cb, ami_ori,
		"Event: VarSet\n"
		"Variable: ami_originate_id\n"
		"Value: %s\n"
		, ami_ori->uuid
	);

	con_debug("sending originate with id %s", ami_ori->uuid);
	ami_action(ami, response_originate, ami_ori,
		"Action: Originate\n"
		"Async: 1\n"
		"Variable: ami_originate_id=%s\n"
		"%s"
		, ami_ori->uuid, buf
	);

	return ami_ori;
}

void ami_originate_free(ami_ori_t *ami) {

}

