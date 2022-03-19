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

#define CLEAR_INPUT { char c; while ((c = getchar()) != '\n' && c != '\r' && c != EOF); }
#define PAUSE { printf("Press any key to continue...\n"); char c; scanf("%c", &c); }

#define MAX_PENDING 5
#define BUFFSIZE 32

typedef struct message
{
	char topic[21];
	char msg[513];
} message;

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

void to_lower(char* c)
{
	while (*c)
	{ 
		if (*c >= 'A' && *c <= 'Z')
			*c = *c - 'A' + 'a';

		c++;
	}
}

topicList* searchTopic(char* topic)
{
	topicList* tp = topics;

	while (tp != NULL)
	{
		if (strcmp(tp->topic, topic) == 0)
			break;

		tp = tp->next;
	}
	
	return tp;
}

void create_topic()
{
	char topic[21];
	
	printf("Enter the topic name: ");
	scanf("%[^\n]", topic);
	to_lower(topic);
	
	CLEAR_INPUT;

	// check for the existence of topic
	topicList* exists = searchTopic(topic);

	// if exists == NULL, we need to add topic to head of list, else print error and return
	if (exists)
	{
		printf("Topic \"%s\" already exists!!\n", topic);
		return;
	}

	exists = calloc(1, sizeof(topicList));
	strcpy(exists->topic, topic);
	exists->next = topics;
	topics = exists;
}

int get_msg(message* msg_to_send)
{
	printf("Enter the topic name: ");
	scanf("%[^\n]", msg_to_send->topic);
	CLEAR_INPUT;
	
	to_lower(msg_to_send->topic);

	// check for the existence of topic
	topicList* exists = searchTopic(msg_to_send->topic);

	// if exists == NULL, we need to report an appropriate error. else get the message and send it to server
	
	if (!exists)
	{
		printf("Topic \"%s\" doesn't exist!! Create it first.\n", msg_to_send->topic);
		return -1;
	}

	memset(msg_to_send->msg, 0, sizeof(msg_to_send->msg));

	// get the message
	printf("Enter the message to send, terminated by newline: ");
	scanf("%512[^\n]", msg_to_send->msg);
	CLEAR_INPUT;

	printf("Sending topic: '%s' with message '%s'...\n", msg_to_send->topic, msg_to_send->msg);

	return 0;
}

void send_file(int sockfd)
{
	message m;

	printf("Enter the topic name: ");
	scanf("%[^\n]", m.topic);
	CLEAR_INPUT;
	
	to_lower(m.topic);

	// check for the existence of topic
	topicList* exists = searchTopic(m.topic);

	// if exists == NULL, we need to report an appropriate error. else read the file and send it to server	
	if (!exists)
	{
		printf("Topic \"%s\" doesn't exist!! Create it first.\n", m.topic);
		return;
	}

	char path[256];
	printf("Enter file path: ");
	scanf("%[^\n]", path);

	CLEAR_INPUT;

	FILE* fp = fopen(path, "r");

	if (fp == NULL)
	{
		perror("file read error");
		return;
	}

	int read;
	char *line;
	size_t len = 0;

	while ((read = getline(&line, &len, fp)) != -1)
	{
		strcpy(m.msg, line);

		if (line != NULL)
			free(line);
		line = NULL;

		m.msg[strlen(m.msg) - 1] = '\0';

		if (write(sockfd, (char*)&m, sizeof(m)) <= 0)
			perror("write to server");
	}

	fclose(fp);
}

void do_task(int sockfd)
{
	// initialise global variables
	topics = NULL;


	int option = -1;
	char c = 0;
	do {
		if (c)
			PAUSE
		else
			c = 1;

		clear_screen();

		// Take input from user
		printf("*********************Producer Panel*********************\n");
		printf("0.\tExit\n");
		printf("1.\tCreate a Topic\n");
		printf("2.\tSend a message\n");
		printf("3.\tSend messages from file\n");

		printf("Select an option: ");
		scanf("%d", &option);

		// clear the buffer and clear screen
		CLEAR_INPUT;
		clear_screen();


		// perform the tasks
		if (option == 1)
		{
			create_topic();
			continue;
		}

		if (option == 2)
		{
			message msg;
			if (get_msg(&msg) == -1)
				continue;

			// send the buffer to broker
			if (write(sockfd, (char*)&msg, sizeof(message)) < 0)
				perror("write error");
			continue;
		}

		if (option == 3)
		{
			send_file(sockfd);
			continue;
		}

	} while (option != 0);
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
