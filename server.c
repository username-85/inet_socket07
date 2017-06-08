#include "util.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

//maxlen for full address
#define MAX_ADDR_LEN (INET6_ADDRSTRLEN + MAX_SERVICE_LEN)
#define SA struct sockaddr

// number of file descriptors processing in one threads
#define CLIFDS_FOR_THREAD 10

// max number of client file descriptors in buffer
#define CLIFDS_MAX 120

// client file descriptors buffer
struct rbuf {
	int elem[CLIFDS_MAX];
	int iget;
	int iput;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} clifds = {.mutex = PTHREAD_MUTEX_INITIALIZER,
            .cond = PTHREAD_COND_INITIALIZER
           };

int nthreads; // number of threads
pthread_mutex_t nthreads_mutex= PTHREAD_MUTEX_INITIALIZER;

int nclifds; // number of client file descriptors
pthread_mutex_t nclifds_mutex = PTHREAD_MUTEX_INITIALIZER;

// add file descriptor to buffer
int clifds_add(int fd);

// add file descriptor to buffer
int clifds_get(void);

// funtion to process client connection in thread
static void * process(void *sfd_in);

// reads from file descriptor and writes back
static void echo(int fd);

static void inc_nthreads(void);
void dec_nthreads(void);
int get_nthreads(void);

void inc_nclifds(void);
void dec_nclifds(void);
int get_nclifds(void);

void debug_print(void);

int main(int argc, char *argv[])
{
	int debug = (argc == 2 && strcmp(argv[1], "-d") == 0);

	socklen_t addrlen = 0;
	int listenfd = inet_listen(PORT_SRV, BACKLOG, &addrlen);
	if (listenfd < 0)
		err_exit("could not create tcp socket\n");

	printf("waiting for connections on tcp port %s\n", PORT_SRV);

	pthread_t tid;
	struct sockaddr_storage cliaddr;

	while (1) {
		socklen_t len = addrlen;
		int connfd = accept(listenfd, (SA *)&cliaddr, &len);
		if (connfd == -1)
			err_sys_exit("accept\n");
		clifds_add(connfd);
		inc_nclifds();

		char buf[MAX_ADDR_LEN] = {0};
		inet_addr_str((SA *)&cliaddr, len, buf, sizeof(buf));
		//printf("got tcp connection from %s\n", buf);

		int nfds = get_nclifds();
		int nthr = get_nthreads();
		if (nthr == 0 || nfds / CLIFDS_FOR_THREAD > nthr) {
			int s = pthread_create(&tid, NULL, &process, NULL);
			if (s != 0)
				err_exit("pthread_create error");
			inc_nthreads();
		}

		if (debug)
			debug_print();
	}

	exit(EXIT_SUCCESS);
}

void * process(void *unused)
{
	(void)unused;
	pthread_detach(pthread_self());

	while (1) {
		int fd = clifds_get();
		echo(fd);
		close(fd);
		dec_nclifds();
	}
	dec_nthreads();

	return NULL;
}

void echo(int fd)
{
	char buf[MAXDSIZE];
	memset(buf, 0, sizeof(buf));
	int bytes = read(fd, buf, sizeof(buf));
	if (bytes > 0)
		write(fd, buf, sizeof(buf));
}

int clifds_add(int fd)
{
	pthread_mutex_lock(&clifds.mutex);

	clifds.elem[clifds.iput] = fd;
	if ( ++clifds.iput == CLIFDS_MAX)
		clifds.iput = 0;

	if ( clifds.iput == clifds.iget)
		err_exit("buffer ERROR! iput index = iget index\n");

	pthread_cond_signal(&clifds.cond);
	pthread_mutex_unlock(&clifds.mutex);
	return 0;
}

int clifds_get(void)
{
	pthread_mutex_lock(&clifds.mutex);

	while (clifds.iget == clifds.iput)
		pthread_cond_wait(&clifds.cond, &clifds.mutex);

	int ret = clifds.elem[clifds.iget];
	if (++clifds.iget == CLIFDS_MAX)
		clifds.iget = 0;

	pthread_mutex_unlock(&clifds.mutex);

	return ret;
}

void inc_nthreads(void)
{
	pthread_mutex_lock(&nthreads_mutex);
	nthreads++;
	pthread_mutex_unlock(&nthreads_mutex);
}

void dec_nthreads(void)
{
	pthread_mutex_lock(&nthreads_mutex);
	nthreads--;
	pthread_mutex_unlock(&nthreads_mutex);
}

int get_nthreads(void)
{
	int ret;
	pthread_mutex_lock(&nthreads_mutex);
	ret = nthreads;
	pthread_mutex_unlock(&nthreads_mutex);
	return ret;
}

void inc_nclifds(void)
{
	pthread_mutex_lock(&nclifds_mutex);
	nclifds++;
	pthread_mutex_unlock(&nclifds_mutex);
}

void dec_nclifds(void)
{
	pthread_mutex_lock(&nclifds_mutex);
	nclifds--;
	pthread_mutex_unlock(&nclifds_mutex);
}

int get_nclifds(void)
{
	int ret;
	pthread_mutex_lock(&nclifds_mutex);
	ret = nclifds;
	pthread_mutex_unlock(&nclifds_mutex);
	return ret;
}

void debug_print(void)
{
	puts("--------------------------debug---------------------------\n");
	printf("CLIFDS_MAX = %d\n", CLIFDS_MAX);
	printf("nthreads = %d\n", get_nthreads());
	printf("nfds = %d\n", get_nclifds());

	//ignoring mutex in clifds for printing
	printf("iget = %d\n", clifds.iget);
	printf("iput = %d\n", clifds.iput);
	for (int i = 0; i < CLIFDS_MAX; i++)
		printf("%d ", clifds.elem[i]);

	puts("\n");
	puts("----------------------------------------------------------\n");
}

