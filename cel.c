/* cel.c
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

#include "ami.h"
#include "debug.h"

#define CON_DEBUG
#include "logger.h"

ev_timer timer;
ev_timer timer_reconnect;

char *host;
ami_t *ami;
FILE *ftarget;

void fprint_joker (char *str) {
    int i;
    int len = strlen(str);
    for (i = 0; i < len; i++) {
        if (str[i] == '\'')
            fputc('\\', ftarget);
        fputc(str[i], ftarget);
    }
}

void event_cel (ami_event_t *event) {
    char timestamp[32];
    time_t tt;
    tt = time(NULL);
    strftime(timestamp, sizeof(timestamp)-1, "%b %d %H:%M:%S", localtime(&tt));

    char *var, *value;
    int i;
    for (i = 0; i < event->field_size; i += 2) {
        var = &event->data[event->field[i]];
        value = &event->data[event->field[i+1]];

        printf("%13s: %s\n", var, value);
    }
    printf("\n");
    fflush(stdout);
}

void event_disconnect (ami_event_t *event) {
    ami_t *ami = event->ami;
    printf("* %s %s(%s:%s): %s\n",
        (ami->authenticated) ? "Disconnected from" : "Can't connect to",
        ami_getvar(event, "Host"),
        ami_getvar(event, "IP"),
        ami_getvar(event, "Port"),
        ami_getvar(event, "Reason"));
    exit(0);
}

void event_connect (ami_event_t *event) {
    printf("* Connected to %s(%s:%s)\n",
        ami_getvar(event, "Host"),
        ami_getvar(event, "IP"),
        ami_getvar(event, "Port"));
}

int main (int argc, char *argv[]) {
    ami = ami_new(EV_DEFAULT);
    if (ami == NULL) {
        con_debug("ami_new() returned NULL");
        return 1;
    }

    if (argc < 2) {
        printf("usage: %s <host>\n", argv[0]);
        exit(1);
    }

    host = argv[1];

    printf("* Connecting to %s ...\n", host);
    ami_credentials(ami, "jsi", "pwd", host, "5038");

    ami_event_register(ami, event_disconnect, NULL, "Disconnect");
    ami_event_register(ami, event_connect, NULL, "Connect");
    ami_event_register(ami, event_cel, NULL, "Event: CEL");

    ami_connect(ami);

    ev_loop(EV_DEFAULT, 0);

    ami_destroy(ami);
    return 0;
}



