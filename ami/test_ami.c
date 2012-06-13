#include <stdio.h>

#include "ami.h"

static void ami_callback (ami_t *ami) {
	char *userdata = (char*)ami->userdata;

	switch (ami->event) {
		case AMI_DISCONNECT:
			printf("Disconnected from %s (%s)\n", ami->netsocket->host, ami->disconnect_reason);
			// schedule_reconnect();
			break;

		case AMI_CONNECTED:
			printf("Connected yeahhhh\n");
			break;

		case AMI_EVENT:
			if (ami_cmp(ami, "Event", "DongleSMSStatus")) {
				if (ami_strcmp(ami, "ID", "%s", status_id)) {
					char *status = ami_getvar(ami, "Status");
				}
			}
			break;
	}
}

static void ami_event_callback (ami_event_t *event) {
	char *userdata = (char*)event->userdata;

	char *status = ami_getvar(event, "Status");

	char status2[64];
	strncpy(status2, ami_getvar(event, "Status"), 64);
	ami_strcpy(event, status2, "Status", 64);

	ami_unsubscribe(event);
}

static void ami_response_callback (ami_event_t *response) {
	char *userdata = (char*)response->userdata;

	int err = response->err;
	char *message = ami_getvar(response, "Message");

}

int main (int argc, char *argv[]) {
	ami_t *ami;

	ami_init(ami, "localhost", 5038, "jsi", "pwd", ami_callback);
	ami_connect(ami);

	ami_event_t *sms_status = ami_event_register(ami, ami_event_callback, userdata,
		"Event: DongleSMSStatus\nID: %s", message_id);
	ami_event_unregister(sms_status);

	ami_event_t *response = ami_action(ami, ami_response_callback, userdata,
		"Action: DongleSendPDU\nDevice: %s\nPDU: %s", device, pdu);

	ami_action(ami, NULL, NULL, "Originate 1212 1853");

	ami_event_unsubscribe(response);

	ami_destroy(ami);


	return 0;
}



