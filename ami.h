/* ami.h
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#ifndef AMI_H_INCLUDED
#define AMI_H_INCLUDED

#include "netsocket.h"

// TODO: doksiba AMI_DEBUG_PACKET_DUMP

#ifndef AMI_DEFAULT_HOST
#define AMI_DEFAULT_HOST "localhost"
#endif

#ifndef AMI_DEFAULT_PORT
#define AMI_DEFAULT_PORT 5038
#endif

#ifndef AMI_BUFSIZ
#define AMI_BUFSIZ 4096
#endif

#ifndef AMI_FIELD_SIZE
#define AMI_FIELD_SIZE 64
#endif

enum ami_event_type {
    AMI_EVENT = 1,
    AMI_RESPONSE,
    AMI_CLIRESPONSE,
    AMI_CONNECT,
    AMI_DISCONNECT,
};

// ha változik, akkor egyeztess az ami.c ami_dump_lists() függvénnyel!
typedef struct ami_event_list_t {
    struct ami_event_list_t *prev;
    struct ami_event_list_t *next;
    void (*callback)(void*);
    void *userdata;
    int field[AMI_FIELD_SIZE];
    int field_size;
    char data[AMI_BUFSIZ];
    char *regby_file;
    int regby_line;
    const char *regby_function;
    const char *regby_cbname;
    const char *regby_udname;
    unsigned int action_id;
    enum ami_event_type type;
    int allevents; // 1 lesz, ha a megrendelo szures nelkul az osszes AMI esemenyt keri
} ami_event_list_t;

typedef struct ami_event_t {
    struct ami_event_t *prev;
    struct ami_event_t *next;
    struct ami_t *ami;
    int success; // csak "Response: Success" esetén lesz egy, tehát biztos hogy volt Response és az értéke Success volt
    int field[AMI_FIELD_SIZE];
    int field_size;
    char data[AMI_BUFSIZ];
    int data_size;
    void (*callback)(void*);
    void *userdata;
    unsigned int action_id;
    char *regby_file;
    int regby_line;
    const char *regby_function;
    const char *regby_cbname;
    const char *regby_udname;
    enum ami_event_type type;
} ami_event_t;

typedef struct ami_t {
    char host[64];                              // Asterisk host
    int port;                                   // Asterisk Manager Interface port
    char username[32];                          // AMI User
    char secret[32];                            // AMI Password
    netsocket_t *netsocket;                     // Netsocket objektum
    char disconnect_reason[64];                 // ???
    ami_event_list_t *ami_event_list_head;      // megrendelt események
    struct ev_loop *loop;                       // eseményhurok
    ev_timer need_event_processing;             // azonnali időzítő az események kiküldéséhez
    ev_timer t_connect_delayed;                 // késleltetett connect időzítő
    char inbuf[AMI_BUFSIZ];                     // bejövő buffer
    int inbuf_pos;                              // bejövő buffer pozíciója
    struct ami_event_t *event_head;             // esemény várakozósor
    struct ami_event_t event_tmp;               // itt készítünk új eseményt, amit aztán a várakozósorba másolunk
    int authenticated;                          // 1 lesz sikeres login után
    unsigned int action_id;                     // soron következő használható ActionID
    char uuid[16];                              // AMI sajat belso ID
    int cli_actionid;                           // process_input() itt jegyzi meg a Response: Follows ActionID-t
} ami_t;

ami_t *ami_new (struct ev_loop *loop);

void ami_credentials (ami_t *ami, const char *username, const char *secret, const char *host, const char *port);

void ami_destroy(ami_t *ami);

void ami_connect (ami_t *ami);

void ami_connect_delayed (ami_t *ami, int delay);

int ami_printf (ami_t *ami, const char *fmt, ...);

#define ami_action(ami, callback, userdata, ...) \
    _ami_action(ami, callback, userdata, __FILE__, __LINE__, __FUNCTION__, #callback, #userdata, __VA_ARGS__)
ami_event_list_t *_ami_action (ami_t *ami, void *callback, void *userdata, char *file, int line, const char *function, const char *cbname, const char *udname, const char *fmt, ...);

#define ami_event_register(ami, callback, userdata, ...) \
    _ami_event_register(ami, callback, userdata, __FILE__, __LINE__, __FUNCTION__, #callback, #userdata, __VA_ARGS__)
ami_event_list_t *_ami_event_register (ami_t *ami, void *callback, void *userdata, char *file, int line, const char *function, const char *cbname, const char *udname, const char *fmt, ...);

void ami_event_unregister(ami_t *ami, ami_event_list_t *el);

char *ami_getvar (ami_event_t *event, char *var);

#define ami_strcpy(event,dest,var) ami_strncpy(event,dest,var,sizeof(dest))

void ami_strncpy (ami_event_t *event, char *dest, char *var, size_t maxsize);

void ami_dump_lists (ami_t *ami);

void ami_event_dump (ami_event_t *el);

#endif // #ifndef AMI_H_INCLUDED

