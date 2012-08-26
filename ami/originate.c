
// TODO: kigyomlálni a nem kellő include-okat
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ev.h>
#include <stdio.h>
#include <stdarg.h>

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

ami_ori_t *ami_originate (ami_t *ami, const char *fmt, ...) {
	ami_ori_t *ami_ori = malloc(sizeof(*ami_ori));
	if (ami_ori == NULL) {
		con_debug("ami_originate() returned NULL");
		return NULL;
	}
	bzero(ami_ori, sizeof(*ami_ori)); // minden NULL

	// AMI UUID
	generate_uuid(ami_ori->uuid, sizeof(ami_ori->uuid));

	return ami_ori;
}

void ami_originate_free(ami_ori_t *ami) {

}

