#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <cassert>
#include <fstream>
#include "helpers/SocketIO.h"

#define MESSAGE_TIME_LIMIT 60
using namespace std;

#define CLEAR_INPUT { char c; while ((c = getchar()) != '\n' && c != '\r' && c != EOF); }
#define PAUSE { printf("Press any key to continue...\n"); char c; scanf("%c", &c); }

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

class Subscriber
{
private:
	const int socket;
	string topic;
	short_time t;

    int getServerResponse(int socket, vector<ClientMessage> &result, string &errMsg) const
	{
		errMsg = "";
		bool isConnectionClosed;
		result = SocketIO::client_readData(socket, errMsg, isConnectionClosed);

        if (isConnectionClosed)
		{
			cout << "Connection closed from server side!!" << endl;
			exit(-1);
		}

        if (result.size() == 0)   // error
        {
			errMsg = "Unknown error occured!!";
			return -1;
		}

		if (strcmp(result[0].req, "OK") == 0)
			return 0;

		if (strcmp(result[0].req, "NTO") == 0)
			errMsg = "Topic doesn't exist on server!!";
		else if (strcmp(result[0].req, "ERR") == 0)
			errMsg = "Invalid request!!";
		else if (strcmp(result[0].req, "TAL") == 0)
			errMsg = "Topic already exist on server";
		else if (strcmp(result[0].req, "NMG") == 0)
			errMsg = "No more messages!!";
		else
			errMsg = "Unknown error occured!!";
		
		return -1;
	}

public:
	Subscriber(int socket) : socket{socket}
	{
		
	}

	void registerTopic(string &topic)
	{
		this->topic = topic;
		this->t = std::chrono::high_resolution_clock::now() - std::chrono::seconds(MESSAGE_TIME_LIMIT);
		cout << DateTime(this->t) << endl;
	}

	int getAllTopics(vector<string> &topics, string &errMsg) const
	{
		bool isConnectionClosed;
		int snd_result = SocketIO::client_writeData(socket, "GAT", "", {}, errMsg, isConnectionClosed);

        if (isConnectionClosed)
		{
			cout << "Connection closed from server side!!" << endl;
			exit(-1);
		}

		// write error
		if (snd_result == -1)
			return -1;

		vector<ClientMessage> arr;
		if (getServerResponse(socket, arr, errMsg) == -1)
			return -1;

		for (auto &x: arr)
			if (strcmp(x.msg, "") != 0)
				topics.push_back(x.msg);
		
		return 0;
	}

	int getNextMessage(string& msg, string &errMsg)
	{
		bool isConnectionClosed;
		int snd_result = SocketIO::client_writeData(socket, "GNM", topic.c_str(), {}, errMsg, isConnectionClosed, t);

        if (isConnectionClosed)
		{
			cout << "Connection closed from server side!!" << endl;
			exit(-1);
		}

		// write error
		if (snd_result == -1)
			return -1;

		vector<ClientMessage> responses;

		if (getServerResponse(socket, responses, errMsg) == -1)
			return -1;

		msg = responses[0].msg;
		t = responses[0].time;
		return 0;
	}

	int getBulkMessages(vector<string> &msgs, string &errMsg)
	{
		bool isConnectionClosed;
		int snd_result = SocketIO::client_writeData(socket, "GAM", topic.c_str(), {}, errMsg, isConnectionClosed, t);

        if (isConnectionClosed)
		{
			cout << "Connection closed from server side!!" << endl;
			exit(-1);
		}

		// write error
		if (snd_result == -1)
			return -1;

		vector<ClientMessage> responses;

		if (getServerResponse(socket, responses, errMsg) == -1)
			return -1;

		for (auto &x: responses)
			msgs.push_back(x.msg);

		t = responses.back().time;
		return 0;
	}
};

void do_task(int sockfd)
{
	Subscriber s(sockfd);

	int option = -1;
	char c = 0;

	while (1)
	{
		if (c)
			PAUSE
		else
			c = 1;

		clear_screen();

		// Take input from user
		cout << "*********************Subscriber Panel*********************" << endl;
		cout << "0.\tExit" << endl;
		cout << "1.\tSubscribe to a Topic" << endl;
		cout << "2.\tRetrieve next message" << endl;
		cout << "3.\tRetrieve all messages" << endl;
		cout << "Select an option: ";
		cin >> option;

		if (option < 0 || option > 3)
			continue;

		if (option == 0)
			break;

		// clear the buffer and clear screen
		CLEAR_INPUT;
		clear_screen();

		// perform the tasks
		if (option == 1)
		{
			string errMsg;
			vector<string> topics;
			
			if (s.getAllTopics(topics, errMsg) == -1)
			{
				cout << errMsg << endl;
				continue;
			}

			cout << "0\t: Go Back" << endl;
			for (int i = 0; i < topics.size(); ++i)
				cout << "Topic " << i + 1 << "\t: "	<< topics[i] << endl;
			
			cout << endl << "Select an option: ";
			int option;
			cin >> option;

			if (option <= 0 || option > topics.size())
				cout << endl << "No Option selected..." << endl;
			else
			{
				s.registerTopic(topics[option - 1]);
				cout << endl << "Topic selected: " << topics[option - 1] << endl;
			}
			
			CLEAR_INPUT;
		}

		if (option == 2)
		{
			string errMsg, msg;

			if (s.getNextMessage(msg, errMsg) == -1)
				cout << errMsg << endl;
			else
				cout << "Message Received: " << msg << endl;
			CLEAR_INPUT;
		}

		if (option == 3)
		{
			string errMsg;
			vector<string> msgs;

			if (s.getBulkMessages(msgs, errMsg) == -1)
				cout << errMsg << endl;
			else
			{
				for (int i = 0; i < msgs.size(); ++i)
					cout << "Message " << i + 1 << ": " << msgs[i] << endl;
			}
			CLEAR_INPUT;
		}

	}
}

int main(int argc, char** argv)
{
	int sockfd;
	struct sockaddr_in servaddr;
	freopen("logs/Subscriber.log", "w", stderr);
	if (argc != 3)
	{
		printf("usage: Subscriber.o <IPaddress> <PORT>\n");
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

	// register as subscriber on server
	char buff[4];
	strcpy(buff, "SUB");
	if (write(sockfd, buff, sizeof(buff)) < 0)
	{
		perror("write error to server");
		exit(1);
	}

	int n = read(sockfd, buff, sizeof(buff));
	if (n < 0)
	{
		perror("read error");
		exit(1);
	}
	if (n == 0)
	{
		cout << "Connection ended prematurely!" << endl;
		exit(1);
	}

	if (strcmp(buff, "OK") == 0)
		cout << "Connection established successfully" << endl;
	else if (strcmp(buff, "ERR") == 0)
	{
		cout << "Invalid request" << endl;
		exit(-1);
	}
	else
	{
		cout << "Unknown error occured" << endl;
		exit(-1);
	}

	do_task(sockfd);
	close(sockfd);

	return 0;
}
