/* meetmetalk.c
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
#include <time.h>
#include <unistd.h>

#include "ami.h"
#include "debug.h"

//~ #define CON_DEBUG
#include "logger.h"

char *host;
ami_t *ami;

void event_disconnect (ami_event_t *event) {
    ami_t *ami = event->ami;
    printf("* %s %s(%s:%s): %s\n",
        (!strcmp(ami_getvar(event, "WasAuthenticated"), "1")) ? "Disconnected from" : "Can't connect to",
        ami_getvar(event, "Host"),
        ami_getvar(event, "IP"),
        ami_getvar(event, "Port"),
        ami_getvar(event, "Reason"));
    ami_connect_delayed(ami, 3000);
}

void event_connect (ami_event_t *event) {
    printf("* Connected to %s(%s:%s)\n",
        ami_getvar(event, "Host"),
        ami_getvar(event, "IP"),
        ami_getvar(event, "Port"));
}

// Sep 17 23:27:09 localhost --- {'Event' => 'MeetmeTalking', 'Privilege' =>
// 'call,all', 'Channel' => 'SIP/199-000004e2', 'Uniqueid' =>
// 'r-pbx-1379452903.2099', 'Meetme' => '1801', 'Usernum' => '1', 'Status' =>
// 'off'}

static void event_meetmetalking (ami_event_t *event) {
    int status = (!strcmp(ami_getvar(event, "Status"), "on")) ? 1 : 0;
    char *meetme = ami_getvar(event, "Meetme");

    // printf("meetme = %s - status = %d\n", meetme, status);

    if (status) {
        system("clear");
        system("figlet -f big \"   ON  AIR\"");
    } else {
        system("clear");
    }
}

int main (int argc, char *argv[]) {
    ami = ami_new(EV_DEFAULT);
    if (ami == NULL) {
        con_debug("ami_new() returned NULL");
        return 1;
    }

    if (argc < 2) {
        printf("JSS MeetMeTalk\n"
               "(c) 2013 JSS&Hayer - http://libamievent.jss.hu\n"
               "\n"
               "usage: %s <host>\n", argv[0]);
        exit(1);
    }

    host = argv[1];

    printf("* Connecting to %s ...\n", host);
    ami_credentials(ami, "jsi", "pwd", host, "5038");
    ami_event_register(ami, event_disconnect, NULL, "Disconnect");
    ami_event_register(ami, event_connect, NULL, "Connect");
    ami_event_register(ami, event_meetmetalking, NULL, "Event: MeetmeTalking");
    ami_connect(ami);

    ev_loop(EV_DEFAULT, 0);

    ami_destroy(ami);
    return 0;
}



