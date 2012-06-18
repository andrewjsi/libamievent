#include <stdio.h>
#include <string.h>
#include <ev.h>

#include "ami.h"

#define CON_DEBUG
#include "logger.h"

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

	ami_event_unregister(ame);
}

static void ami_response_callback (ami_event_t *response) {
	char *userdata = (char*)response->userdata;

	int err = response->err;
	char *message = ami_getvar(response, "Message");

}

static void ami_login_response_success (ami_event_t *response) {
	con_debug("logged in: %s (by %s() %s:%d)",
		ami_getvar(response, "Message"),
		response->invokedby->stack_function,
		response->invokedby->stack_file,
		response->invokedby->stack_line
	);
}

int main (int argc, char *argv[]) {
	ami_t *ami;

	ami = ami_new(ami_callback, NULL, EV_DEFAULT);
	if (ami == NULL) {
		con_debug("ami_new() returned NULL");
		return 1;
	}

	if (argc < 2) {
		printf("usage: %s <host>\n", argv[0]);
		return 1;
	}

	printf("Connecting to %s ...\n", argv[1]);
	ami_credentials(ami, "jsi", "pwd", argv[1], "5038");
	ami_connect(ami);

	char *userdata = "juzeradat";
	char *message_id = "V59";

	ami_event_t *sms_status = ami_event_register(ami, ami_event_callback, userdata,
		"Event: DongleSMSStatus\nID: %s", message_id);

	ami_event_t *sms_status3 = ami_event_register(ami, ami_event_callback, userdata,
		"egy\nketto\nharom", message_id);

	ami_event_register(ami, ami_login_response_success, NULL, "Response: Success");

	//~ ami_event_unregister(sms_status);

	char *device = "dongle0";
	char *pdu = "ABCDEF1234";

	ami_event_t *response = ami_action(ami, ami_response_callback, userdata,
		"Action: DongleSendPDU\nDevice: %s\nPDU: %s", device, pdu);

	ami_action(ami, NULL, NULL, "Originate 1212 1853");

	ami_event_unregister(response);

	printf("\n");
	ami_dump_lists(ami);

	ev_loop(EV_DEFAULT, 0);

	ami_destroy(ami);

	return 0;
}



