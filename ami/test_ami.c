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

static void ami_event_callback (ami_event_t *ae) {
	char *userdata = (char*)ae->userdata;

	char *status = ami_getvar(ae->vars, "Status");

	char status2[64];
	strncpy(status2, ami_getvar(ae->vars, "Status"), 64);
	ami_strcpy(ae, status2, "Status", 64);

	ami_unsubscribe(ae);
}

static void ami_action_callback (ami_action_t *aa) {
	char *userdata = (char*)aa->userdata;

	int success = aa->err;
	char *message = ami_getvar(aa->vars, "Message");

}

int main (int argc, char *argv[]) {
	ami_t *ami;

	ami_init(ami, "localhost", 5038, "jsi", "pwd", ami_callback);
	ami_connect();

	ami_event_t *sms_status = ami_subscribe(ami, ami_event_callback, userdata,
		"Event: DongleSMSStatus\nID: %s", message_id);
	ami_unsubscribe(sms_status);

	ami_action_t *action = ami_action(ami, ami_action_callback, userdata,
		"Action: DongleSendPDU\nDevice: %s\nPDU: %s", device, pdu);

	ami_action_unsubscribe(action);

	ami_destroy(ami);


	return 0;
}



