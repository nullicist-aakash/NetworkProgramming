/**********************************************************************
 * Aakash
 * 2018B4A70887P
 * Lab 4 Submission
 * ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define MAX_PENDING 5
#define BUFFERSIZE 32

typedef void Sigfunc(int);

typedef struct
{
	int type;
	char c;
} my_msg;

int qid;

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

void do_task(int connfd, int qid, struct sockaddr_in *cliaddr, socklen_t clilen)
{
	int n;
	char buff[256];
	printf("Handling client: %s\n",  inet_ntoa(cliaddr->sin_addr));

here:
	n = read(connfd, buff, 256);

	if (n < 0)
	{
		perror("read error");
		my_msg msg;
		msg.type = 1;

		// send the signal for parent that a child is terminated
		msgsnd(qid, &msg, sizeof(my_msg) - sizeof(long), 0);
		
		exit(-1);
	}

	if (n == 0)	// connection closed from client side
	{
		my_msg msg;
		msg.type = 1;

		// send the signal for parent that a child is terminated
		msgsnd(qid, &msg, sizeof(my_msg) - sizeof(long), 0);
		
		exit(-1);
	}

	buff[n] = '\0';
	printf("%s: %s\n", inet_ntoa(cliaddr->sin_addr), buff);

	goto here;
}

void main(int argc, char** argv)
{
	// get IPC key
	key_t mykey = ftok(".", 'a');

	// create message queue
	qid = msgget(mykey, IPC_CREAT | 0660);
	if (qid == -1)
	{
		perror("Error getting access to message queue");
		exit(-1);
	}

	int listenfd;
	struct sockaddr_in servaddr;
	int N;

	if (argc != 3)
	{
		printf("usage: server.o <PORT number> <N>\n");
		exit(1);
	}

	// create the CLOSED state
	listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	N = atoi(argv[2]);

	if (listenfd < 0)
	{
		perror("socket error");
		exit(1);
	}

	// bind the server to PORT
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(atoi(argv[1]));

	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		perror("binding error");
		exit(1);
	}
	

	printf("Listening to PORT %d...\n", ntohs(servaddr.sin_port));

	// move from CLOSED to LISTEN state, create passive socket
	if (listen(listenfd, 5) < 0)
	{
		perror("listen error");
		exit(1);
	}

	// Attach the signal handler
	
	Signal(SIGCHLD, sig_child);
	int connection_count = 0;

	// wait for connections
	while (1)
	{
		struct sockaddr_in cliaddr;
		socklen_t clilen = sizeof(cliaddr);
		int connfd;
		pid_t childpid;

		// check if more client requests can be handled or not
		while (connection_count != 0)
		{
			my_msg msg;
			int rcv = msgrcv(qid, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT);
			
			// Queue is empty
			if (rcv == -1 && errno == ENOMSG)
				break;

			if (rcv == -1)
			{
				perror("queue read error");
				exit(-1);
			}

			// no error while receiving the message
			connection_count--;
		}

		// do three way handshake
		if ((connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen)) < 0)
			if (errno == EINTR)
				continue;
			else
				perror("accept error");

		if (connection_count >= N)
		{
			printf("Dropping new connection...\n");

			// close the connection
			close(connfd);
			continue;
		}

		// fork and process
		if ((childpid = fork()) == 0)
		{
			close(listenfd);

			do_task(connfd, qid, (struct sockaddr_in*)&clilen, sizeof(clilen));
			exit(0);
		}

		close(connfd);
		connection_count++;
		printf("Conections established: %d out of %d\n", connection_count, N);
	}
}
