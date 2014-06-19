/* test_ami2.c
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#include <stdio.h>
#include <string.h>
#include <ev.h>
#include <stdlib.h>

#include "ami.h"
#include "debug.h"

#define CON_DEBUG
#include "logger.h"

ami_t *ami;

void event_connect (ami_event_t *event) {
    printf("*** CONNECTED ***\n");
    printf("  semmi   = %s\n  host    = %s\n  ip      = %s\n  port    = %s\n",
        ami_getvar(event, "semmi"),
        ami_getvar(event, "Host"),
        ami_getvar(event, "IP"),
        ami_getvar(event, "Port"));
}

void event_disconnect (ami_event_t *event) {
    printf("*** DISCONNECTED ***\n");
    printf("  reason  = %s\n  host    = %s\n  ip      = %s\n  port    = %s\n",
        ami_getvar(event, "Reason"),
        ami_getvar(event, "Host"),
        ami_getvar(event, "IP"),
        ami_getvar(event, "Port"));
}

void event_dial123 (ami_event_t *event) {
    ami_action(ami, NULL, NULL, "Action: Logoff");
}

void event_dial124 (ami_event_t *event) {
    ami_action(ami, NULL, NULL, "Action: Hangup\nChannel: %s", ami_getvar(event, "Channel"));
}

int main (int argc, char *argv[]) {
    ami = ami_new(EV_DEFAULT);
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


    ami_event_register(ami, event_connect, NULL, "Connect");
    ami_event_register(ami, event_disconnect, NULL, "Disconnect");

    printf("\n");
    ami_dump_lists(ami);

    ev_loop(EV_DEFAULT, 0);

    ami_destroy(ami);

    return 0;
}



