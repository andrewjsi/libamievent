#include <stdio.h>
#include <string.h>
#include <ev.h>
#include <stdlib.h>

#include "ami.h"
#include "debug.h"
#include "utlist.h"

#define CON_DEBUG
#include "logger.h"

ev_timer timer;
ami_t *ami;
int dstatus_n = 100;

static void ami_callback (ami_event_t *ame) {
	char *userdata = (char*)ame->userdata;

	switch (ame->type) {
		case AMI_DISCONNECT:
			conft("Disconnected from %s (%s)\n", ame->ami->netsocket->host, ame->ami->disconnect_reason);
			// schedule_reconnect();
			break;

		case AMI_CONNECT:
			conft("Connected yeahhhh\n");
			break;

		default:
			conft("Broaf\n");
			break;
	}
}

static void ami_event_callback (ami_event_t *ame) {
	char *userdata = (char*)ame->userdata;
	char *status = ami_getvar(ame, "Status");

	char status2[64];
	strncpy(status2, ami_getvar(ame, "Status"), 64);
	ami_strncpy(ame, status2, "Status", 64);

	//~ ami_event_unregister(ame);
}

static void ami_response_callback (ami_event_t *response) {
	char *userdata = (char*)response->userdata;

	int succ = response->success;
	char *message = ami_getvar(response, "Message");

}

static void ami_login_response_success (ami_event_t *response) {
	con_debug("logged in: %s (by %s() %s:%d)",
		ami_getvar(response, "Message"),
		response->regby_function,
		response->regby_file,
		response->regby_line
	);
	//~ ami_printf(response->ami, "Action: Listcommands\nActionID: 59");
}

void utproba () {
	typedef struct st {
		struct st *prev;
		struct st *next;
		char str[32];
	} st;

	st *head = NULL;
	st *el;
	st *x10, *x15;

	int i;
	for (i = 0; i < 20; i++) {
		el = (st*)malloc(sizeof(*el));
		bzero(el, sizeof(*el));

		sprintf(el->str, "str %d", i);
		if (i == 10)
			x10 = el;
		if (i == 15)
			x15 = el;
		DL_APPEND(head, el);
	}

	st *eltmp;
	DL_FOREACH_SAFE(head, el, eltmp) {
		if (el == x10) {
			st *csere = (st*)malloc(sizeof(st));
			strcpy(csere->str, "csere1");
			DL_APPEND(head, csere);
			csere = (st*)malloc(sizeof(st));
			strcpy(csere->str, "csere2");
			DL_APPEND(head, csere);

			DL_DELETE(head, el);
			free(el);
		}

		if (el == x15) {
			DL_DELETE(head, el);
			free(el);
		}

	}

	DL_FOREACH_SAFE(head, el, eltmp) {
		printf("s %s s\n", el->str);
		DL_DELETE(head, el);
		free(el);
	}

	return;
}

void event_dial (ami_event_t *event) {
	printf("From: %s (%s) To: %s\n",
		ami_getvar(event, "CallerIDNum"),
		ami_getvar(event, "CallerIDName"),
		ami_getvar(event, "Dialstring"));
}

void event_rtcpreceived (ami_event_t *event) {
	printf("*** RTCP received: IAJitter=%s DLSR=%s\n",
		ami_getvar(event, "IAJitter"),
		ami_getvar(event, "DLSR"));
}

void event_fullybooted (ami_event_t *event) {
	printf("*** Fully booted ***\n");
}

void response_dongleshowdevices (ami_event_t *event) {
	printf("dongle_response\n");
}

static void timer_dstatus (EV_P_ ev_timer *w, int revents) {
	ami_action(ami, response_dongleshowdevices, NULL, "Action: DongleShowDevices");
	dstatus_n--;
	if (dstatus_n <= 0) {
		printf("***** DISABLING DSTATUS TIMER ******\n");
		ev_timer_stop(EV_DEFAULT, w);
	}
}


int main (int argc, char *argv[]) {
	ami = ami_new(ami_callback, NULL, EV_DEFAULT);
	if (ami == NULL) {
		con_debug("ami_new() returned NULL");
		return 1;
	}

	char host[128];

	if (argc < 2) {
		//~ printf("usage: %s <host>\n", argv[0]);
		printf("Default host used!\n");
		strcpy(host, "10.27.1.222");
	} else {
		strcpy(host, argv[1]);
	}

	printf("Connecting to %s ...\n", host);
	ami_credentials(ami, "jsi", "pwd", host, "5038");
	ami_connect(ami);

	char *userdata = "juzeradat";
	char *message_id = "V59";

	ami_event_list_t *sms_status = ami_event_register(ami, ami_event_callback, userdata,
		"Event: DongleSMSStatus\nID: %s", message_id);

	ami_event_list_t *sms_status3 = ami_event_register(ami, ami_event_callback, userdata,
		"egy\nketto\nharom", message_id);

	ami_event_register(ami, ami_login_response_success, NULL, "Response: Success");

	ami_event_register(ami, event_dial, NULL, "Event: Dial\n");
	ami_event_register(ami, event_rtcpreceived, NULL, "Event: RTCPReceived\n");

	ami_event_register(ami, event_fullybooted, NULL, "Event: FullyBooted");
	ami_event_register(ami, event_fullybooted, NULL, "Event: FullyBooted\nPrivilege: system,all");

	//~ ami_event_unregister(sms_status);

	char *device = "dongle0";
	char *pdu = "ABCDEF1234";

	//~ ami_event_list_t *response = ami_action(ami, ami_response_callback, userdata,
		//~ "Action: DongleSendPDU\nDevice: %s\nPDU: %s", device, pdu);

	ami_action(ami, NULL, NULL, "Originate 1212 1853");
	//~ ami_event_unregister(response);

	ev_timer tmr;
	ev_timer_init(&tmr, timer_dstatus, 1, 1);
	ev_timer_start(EV_DEFAULT, &tmr);


	printf("\n");
	ami_dump_lists(ami);

	ev_loop(EV_DEFAULT, 0);

	ami_destroy(ami);

	return 0;
}



