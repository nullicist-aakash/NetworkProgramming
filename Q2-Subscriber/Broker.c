#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define MAX_PENDING 5
#define BUFFERSIZE 32

typedef void Sigfunc(int);

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

void do_task(int connfd, struct sockaddr *cliaddr, socklen_t clilen)
{

}

void main(int argc, char** argv)
{
	int listenfd;
	struct sockaddr_in servaddr;

	if (argc != 2)
	{
		printf("usage: server.o <PORT number>\n");
		exit(1);
	}

	// create the CLOSED state
	listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

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

	// wait for connections
	while (1)
	{
		struct sockaddr_in cliaddr;
		socklen_t clilen = sizeof(cliaddr);
		int connfd;
		pid_t childpid;

		// do three way handshake
		if ((connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen)) < 0)
			if (errno == EINTR)
				continue;
			else
				perror("accept error");


		// fork and process
		if ((childpid = fork()) == 0)
		{
			close(listenfd);

			do_task(connfd, (struct sockaddr*)&clilen, sizeof(clilen));
			
			exit(0);
		}

		close(connfd);
	}
}
