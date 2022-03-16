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
#define BUFFSIZE 32

typedef struct topicList
{
	char topic[21];
	struct topicList* next;
} topicList;

topicList *topics;

void clear_screen()
{
#ifdef _WIN32
	system("cls");
#endif
#ifdef _WIN64
	system("cls");
#endif
#ifdef __unix__
	system("clear");
#endif
#ifdef __linux__
	system("clear");
#endif
}

void create_topic()
{
	char topic[21];
	printf("Enter the topic name: ");
	scanf("%s\n", topic);
}

int get_msg(char** topic, char** message)
{

} 

void send_file()
{
}

void do_task(int sockfd)
{
	// initialise global variables
	topics = NULL;


	int option = -1;
	char c;
	do {
		// Take input from user
		printf("*********************Producer Panel*********************\n");
		printf("1.\tCreate a Topic\n");
		printf("2.\tSend a message\n");
		printf("3.\tSend messages from file\n");
		printf("-1\texit\n");

		printf("Select an option ");
		scanf("%d", &option);

		// clear the buffer and clear screen
		while ((c = getchar()) != '\n' && c != EOF);
		clear_screen();


		// perform the tasks
		if (option == 1)
		{
			create_topic();
			continue;
		}

		if (option == 2)
		{
			char** topic;
			char** msg;
			
			if (get_msg(topic, msg) == -1)
				continue;
			
			// prepare the message to send to server
			char buffer[512];
			memset(buffer, 0, sizeof(buffer));
			
			strcpy(buffer + 0, *topic);
			strcpy(buffer + strlen(*topic) + 1, *msg);

			// send the buffer to broker
			write(sockfd, buffer, sizeof(buffer));

			continue;
		}

		if (option == 3)
		{
			send_file();
			continue;
		}

	} while (option != -1);
}

void main(int argc, char** argv)
{
	int sockfd;
	struct sockaddr_in servaddr;

	if (argc != 3)
	{
		printf("usage: Producer.o <IPaddress> <PORT>\n");
		exit(1);
	}

	// create socket in CLOSED state
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// fill the server details
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	servaddr.sin_addr.s_addr = inet_addr(argv[1]);

	// perform three way handshake
	if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		perror("connect error");
		exit(1);
	}

	do_task(sockfd);
	close(sockfd);

	return;
}
