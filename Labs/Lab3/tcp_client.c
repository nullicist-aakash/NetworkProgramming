#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>

#define MAX_PENDING 5
#define BUFFSIZE 32

double commTime = 0;

void do_task(int sockfd)
{
	// send data here
	char buff[2];
	strcpy(buff, "1");
	
	clock_t start_time = clock();

	int n = write(sockfd, buff, 2);
	n = read(sockfd, buff, sizeof(buff));
		
	commTime += (double)(clock() - start_time);
}

void main(int argc, char** argv)
{
	int sockfd;
	struct sockaddr_in servaddr;

	if (argc != 3)
	{
		printf("usage: client.o <URL> <PORT>\n");
		exit(1);
	}

	// get IP address from URL
	struct hostent* hostinfo = gethostbyname(argv[1]);
	if (hostinfo == NULL)
	{
		printf("gethostbyname error for host: %s: %s", hostinfo, hstrerror(h_errno));
		exit(1);
	}

	printf("Official hostname: %s\n", hostinfo->h_name);

	// fill the server details
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	memcpy(&servaddr.sin_addr, *(struct in_addr**)hostinfo->h_addr_list, sizeof(struct in_addr));

	for (int i = 0; i < 100; ++i)
	{
		sockfd = socket(AF_INET, SOCK_STREAM, 0);

		if (sockfd < 0)
		{
			perror("socket error");
			exit(-1);
		}

		// perform three way handshake
		if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
		{
			perror("connect error");
			exit(1);
		}
		else
			printf("Connection success count: %d\n", i + 1);

		do_task(sockfd);
		close(sockfd);
	}

	printf("Average Time: %f seconds.\n", commTime / (100 * CLOCKS_PER_SEC));

	return;
}
