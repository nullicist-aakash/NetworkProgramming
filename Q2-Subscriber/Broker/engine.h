#pragma once
typedef struct connectionInfo
{
	int connfd;
	struct sockaddr_in addr;
	socklen_t addrlen;
} connectionInfo;

/*#include <sys/socket.h>

void server_task(int);

void neighbor_task(int, sockaddr*, socklen_t);

void serve_subscriber(int, sockaddr*, socklen_t);

void serve_producer(int, sockaddr*, socklen_t);*/
