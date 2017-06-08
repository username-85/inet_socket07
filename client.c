#include "util.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

struct msg {
	char ip[INET6_ADDRSTRLEN];
	char data[MAXDSIZE];
} msg;

// send and recieve message to server
void * client(void *msg);

int main(int argc, char *argv[])
{
	char *usage_msg = "usage: client ip \"message\" numthreads\n";
	if (argc < 4)
		err_exit(usage_msg);

	strncpy(msg.ip, argv[1], INET6_ADDRSTRLEN - 1);
	msg.data[MAXDSIZE - 1] = '\0';

	strncpy(msg.data, argv[2], MAXDSIZE - 1);
	msg.data[MAXDSIZE - 1] = '\0';

	int nthreads = atoi(argv[3]);
	for (int i = 0; i  < nthreads; i++) {
		pthread_t tid;
		int s = pthread_create(&tid, NULL, &client, &msg);
		if (s != 0)
			err_exit("pthread_create error");
	}
	sleep(1);

	exit(EXIT_SUCCESS);
}

void * client(void *msg)
{
	pthread_detach(pthread_self());

	struct msg *climsg = msg;

	int srv_sfd = inet_connect(climsg->ip, PORT_SRV, SOCK_STREAM);
	if (srv_sfd < 0) {
		fprintf(stderr, "socket error\n");
		return NULL;
	}

	if (send(srv_sfd, msg, strnlen(msg, MAXDSIZE), 0) == -1) {
		fprintf(stderr, "send error\n");
		goto end;
	}

	char buf[MAXDSIZE] = {0};
	int numbytes = recv(srv_sfd, buf, MAXDSIZE - 1, 0);
	if (numbytes == -1) {
		fprintf(stderr, "recv error\n");
		goto end;
	}

	if (numbytes != MAXDSIZE - 1)
		puts("not all data recieved error\n");
end:
	close(srv_sfd);
	return NULL;
}

