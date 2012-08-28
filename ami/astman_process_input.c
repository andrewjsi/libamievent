#include <stdio.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

#define MAX_HEADERS 80
#define MAX_LEN 256

struct message {
	unsigned int hdrcount;
	char headers[MAX_HEADERS][MAX_LEN];
};

static struct ast_mansession {
	struct sockaddr_in sin;
	int fd;
	char inbuf[MAX_LEN];
	int inlen;
} session;

static char *get_header(struct message *m, char *var)
{
	char cmp[80];
	int x;
	snprintf(cmp, sizeof(cmp), "%s: ", var);
	for (x=0;x<m->hdrcount;x++)
		if (!strncasecmp(cmp, m->headers[x], strlen(cmp)))
			return m->headers[x] + strlen(cmp);
	return "";
}

static int process_message(struct ast_mansession *s, struct message *m)
{
	int x;
	char event[80] = "";
	strncpy(event, get_header(m, "Event"), sizeof(event) - 1);
	if (!strlen(event)) {
		fprintf(stderr, "Missing event in request");
		return 0;
	}

	for (x=0;x<m->hdrcount;x++) {
		printf("Header: %s\n", m->headers[x]);
	}

	return 0;
}

static int get_input(struct ast_mansession *s, char *output)
{
	/* output must have at least sizeof(s->inbuf) space */
	int res;
	int x;
	struct timeval tv = {0, 0};
	fd_set fds;
	for (x=1;x<s->inlen;x++) {
		if ((s->inbuf[x] == '\n') && (s->inbuf[x-1] == '\r')) {
			/* Copy output data up to and including \r\n */
			memcpy(output, s->inbuf, x + 1);
			/* Add trailing \0 */
			output[x+1] = '\0';
			/* Move remaining data back to the front */
			memmove(s->inbuf, s->inbuf + x + 1, s->inlen - x);
			s->inlen -= (x + 1);
			return 1;
		}
	}
	if (s->inlen >= sizeof(s->inbuf) - 1) {
		fprintf(stderr, "Dumping long line with no return from %s: %s\n", inet_ntoa(s->sin.sin_addr), s->inbuf);
		s->inlen = 0;
	}
	FD_ZERO(&fds);
	FD_SET(s->fd, &fds);
	res = select(s->fd + 1, &fds, NULL, NULL, &tv);
	if (res < 0) {
		fprintf(stderr, "Select returned error: %s\n", strerror(errno));
	} else if (res > 0) {
		res = read(s->fd, s->inbuf + s->inlen, sizeof(s->inbuf) - 1 - s->inlen);
		if (res < 1)
			return -1;
		s->inlen += res;
		s->inbuf[s->inlen] = '\0';
	} else {
		return 2;
	}
	return 0;
}

static int input_check(struct ast_mansession *s, struct message **mout)
{
	static struct message m;
	int res;

	if (mout)
		*mout = NULL;

	for(;;) {
		res = get_input(s, m.headers[m.hdrcount]);
		if (res == 1) {
#if 0
			fprintf(stderr, "Got header: %s", m.headers[m.hdrcount]);
			fgetc(stdin);
#endif
			/* Strip trailing \r\n */
			if (strlen(m.headers[m.hdrcount]) < 2)
				continue;
			m.headers[m.hdrcount][strlen(m.headers[m.hdrcount]) - 2] = '\0';
			if (!strlen(m.headers[m.hdrcount])) {
				if (mout && strlen(get_header(&m, "Response"))) {
					*mout = &m;
					return 0;
				}
				if (process_message(s, &m))
					break;
				memset(&m, 0, sizeof(m));
			} else if (m.hdrcount < MAX_HEADERS - 1)
				m.hdrcount++;
		} else if (res < 0) {
			return -1;
		} else if (res == 2)
			return 0;
	}
	return -1;
}



int main () {

	return 0;

}
