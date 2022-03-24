#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <arpa/inet.h>

#define MAX_PENDING 5
#define BUFFERSIZE 32

typedef void Sigfunc(int);

typedef struct message
{
	char topic[21];
	char msg[513];
} message;

typedef struct my_msgbuf
{
	long type;
	int msg;
} my_msgbuf;

typedef struct connectionInfo
{
	int connfd;
	struct sockaddr_in cliaddr;
} connectionInfo;

Sigfunc* Signal(int signo, Sigfunc* func)
{
	struct sigaction act, oldact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sigaction(signo, &act, &oldact) < 0)
		return SIG_ERR;

	return oldact.sa_handler;
}

void sig_child(int signo)
{
	pid_t pid;
	int stat;

	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
		printf("Child terminated: %d\n", pid);
}

int queueID;
int createQueue()
{
	key_t queueKey = ftok(".", 'a');

	if (queueKey == -1)
	{
		perror("ftok");
		exit(-1);
	}

	int id = msgget(queueKey, IPC_CREAT | 0660);

	if (id == -1)
	{
		perror("msgget");
		exit(-1);
	}

	return id;
}

int* readConfigFile(char* path, int *n)
{
	FILE* fp = fopen(path, "r");
	if (fp == NULL)
	{
		perror("erorr opening server config file");
		exit(-1);
	}

	fscanf(fp, "%d", n);

	int *PORTS = calloc(*n, sizeof(int));
	for (int i = 0; i < *n; ++i)
		fscanf(fp, "%d", PORTS + i);

	fclose(fp);

	return PORTS;
}

void* acceptNeighborConnection(void* data)
{
	int listenfd = *(int*)data;

	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int connfd;
	pid_t childpid;

restart:

	// do three way handshake
	if ((connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen)) < 0)
		if (errno == EINTR)
			goto restart;
		else
			perror("accept error");

	connectionInfo* info = calloc(1, sizeof(connectionInfo));
	info->connfd = connfd;
	info->cliaddr = cliaddr;

	return (void*)info;
}

void do_task(int connfd, struct sockaddr *cliaddr, socklen_t clilen)
{
again:
	message m;
	int n = read(connfd, (char*)&m, sizeof(m));

	if (n <= 0)
	{
		perror("read error");
		exit(-1);
	}

	printf("Received %d bytes: { Topic: '%s', Message: '%s' }\n", n, m.topic, m.msg);
	goto again;
}

void load_server(int PORT, int left_port, int right_port)
{
	// create the CLOSED state
	struct sockaddr_in servaddr;
	int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (listenfd < 0)
	{
		perror("socket error");
		exit(-1);
	}

	// bind the server to PORT
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);

	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		perror("binding error");
		exit(1);
	}
	
	// move from CLOSED to LISTEN state, create passive socket
	if (listen(listenfd, 5) < 0)
	{
		perror("listen error");
		exit(1);
	}
	
	printf("Listening to PORT %d...\n", ntohs(servaddr.sin_port));
	
	// signal parent for presence
	my_msgbuf msg;
	msg.type = 1;
	msg.msg = getpid();

	if (msgsnd(queueID, &msg, sizeof(msg) - sizeof(long), 0) == -1)
	{
		perror("sending confirmation");
		exit(-1);
	}

	// wait for the signal from parent to start making connections

	if (msgrcv(queueID, &msg, sizeof(msg) - sizeof(long), getpid(), 0) == -1)
	{
		perror("Receiving make connections");
		exit(-1);
	}

	// wait for connection asynchronously
	pthread_t thread;
	pthread_create(&thread, NULL, &acceptNeighborConnection, (void*)&listenfd);


	// make connection request to a server
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(left_port);
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		perror("connect error");
		exit(-1);
	}

	printf("%d <--> %d\n", PORT, ntohs(servaddr.sin_port));

	// wait for connection
	connectionInfo* info;
	pthread_join(thread, (void**)&info);
	int connfd = info->connfd;
	struct sockaddr_in cliaddr = info->cliaddr;
	
	free(info);
}

void main(int argc, char** argv)
{
	if (argc != 2)
	{
		printf("usage: server.o <CONFIG File>\n");
		exit(1);
	}

	int n;
	int* PORTS = readConfigFile(argv[1], &n);

	queueID = createQueue();

	Signal(SIGCHLD, sig_child);	
	
	pid_t *pids = calloc(n, sizeof(pid_t));

	// make n children
	for (int i = 0; i < n; ++i)
		if ((pids[i] = fork()) == 0)
		{
			load_server(PORTS[i], PORTS[(i + 1) % n], PORTS[(i - 1 + n) % n]);
			exit(-1);
		}
		else if (pids[i] == -1)
		{
			perror("fork");
			exit(-1);
		}

	free(PORTS);

	// wait for confirmations from each server to trigger them to make connection
	for (int i = 0; i < n; ++i)
	{
		my_msgbuf msg;
		if (msgrcv(queueID, &msg, sizeof(msg) - sizeof(long), 1, 0) == -1)
		{
			perror("getting confirmation");
			exit(-1);
		}
	}

	// n servers established. Now, we need to make connections

	for (int i = 0; i < n; ++i)
	{
		my_msgbuf msg;
		msg.type = pids[i];
		
		if (msgsnd(queueID, &msg, sizeof(msg) - sizeof(long), 0) == -1)
		{
			perror("Sending make connections");
			exit(-1);
		}
	}

	free(pids);

	// wait for connections
	/*while (1)
	{
		// fork and process
		if ((childpid = fork()) == 0)
		{
			close(listenfd);

			do_task(connfd, (struct sockaddr*)&clilen, sizeof(clilen));
			
			exit(0);
		}

		close(connfd);
	}*/
}
