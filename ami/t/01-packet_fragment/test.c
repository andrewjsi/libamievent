#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/time.h>

#define PORT 5038

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, clilen;
     struct sockaddr_in serv_addr, cli_addr;

	if (argc < 2) {
		printf("usage: %s <data file>\n", argv[0]);
		return 1;
	}

	char *datafile = argv[1];

again:
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0)
        error("ERROR opening socket");

	int optval = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
		perror("setsockopt"); // TODO normális hibakezelés
		return -1;
	}

     bzero((char *) &serv_addr, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(PORT);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0)
              error("ERROR on binding");

     listen(sockfd,5);
     printf("Listening on port %d ...\n", PORT);
     clilen = sizeof(cli_addr);
     newsockfd = accept(sockfd,
                 (struct sockaddr *) &cli_addr,
                 &clilen);
     if (newsockfd < 0)
          error("ERROR on accept");

     //~ bzero(buffer,256);
     //~ n = read(newsockfd,buffer,255);
     //~ if (n < 0) error("ERROR reading from socket");
     //~ printf("Here is the message: %s\n",buffer);
     //~ n = write(newsockfd,"I got your message",18);
     //~ if (n < 0) error("ERROR writing to socket");


	//~ int seq[] = {27, 55, 50, 1560, 70, 1200, 90, 100, 200, 374, 0}; // 0 kell a végére!

	int seq[5002];
	struct timeval tv;

	gettimeofday(&tv, NULL);
	srand(tv.tv_usec * tv.tv_sec);
	int rr;
	for (rr = 0; rr < 5000; rr++) {
		seq[rr] = rand();
		seq[rr] = seq[rr] % 2000;
		seq[rr]++;
	}
	seq[5001] = 0;
	printf("int seq[] = {");
	for (rr = 0; rr < 5001; rr++) {
		printf("%d, ", seq[rr]);
	}
	printf("};\n");



	int seqn;
	char buf[4096];
	FILE *f;
	int rv;

	f = fopen(datafile, "r");
	if (f == NULL) {
		perror("fopen");
		return 1;
	}

	for (seqn = 0; seq[seqn] != 0; seqn++) {
		//~ printf("Reading %d bytes\n", seq[seqn]);
		rv = fread(buf, 1, seq[seqn], f);
		if (rv <= 0) {
			perror("fread");
			break;
		}
		write(1, buf, rv); // STDOUT
		if (send(newsockfd, buf, rv, MSG_NOSIGNAL) < 0) {
			perror("send");
			break;
		}
		//~ sleep(2);
		usleep(10000); // 100ms
	}

	printf("\n\n");

	close(newsockfd);
	close(sockfd);
	goto again;
	return 0;

}

