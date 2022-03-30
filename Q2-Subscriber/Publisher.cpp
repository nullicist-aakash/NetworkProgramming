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
#include <cassert>
#include <fstream>
#include "SocketIO.h"

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

class Topics
{
private:
	struct topicList
	{
		string topic;
		topicList* next;

		topicList(const string& topic = "", topicList* nxt = nullptr)
		{
			this->topic = topic;
			this->next = nxt;
		}
	};

	topicList* list;

public:
	Topics()
	{

	}

	static bool isValidTopic(const string& topic)
	{
		return topic.size() <= maxTopicSize;
	}

	int addTopic(const string& topic, string& errMsg)
	{
		errMsg = "";
		if (!isValidTopic(topic))
		{
			errMsg = "Topic '" + string(topic) + "' length should be less than " + to_string(maxTopicSize + 1) + " characters";
			return -1;
		}

		list = new topicList(topic, list);
		return 0;
	}

	~Topics()
	{
		while (list != nullptr)
		{
			auto temp = list->next;
			delete list;
			list = temp;
		}
	}
};

class Publisher
{
private:
	const int socket;
	Topics topics;

	int getServerResponse(string &message)
	{
		message = "";
		bool isConnectionClosed;
		auto res = SocketIO::readClientData(socket, message, isConnectionClosed);

        if (isConnectionClosed)
		{
			cout << "Connection closed from server side!!" << endl;
			exit(-1);
		}

        if (res.size() == 0)   // error
            return -1;

		if (strcmp(res[0].req, "OK") == 0)
			return 0;

		if (strcmp(res[0].req, "NTO") == 0)
			message = "Topic doesn't exist on server!! Create topic first";
		else if (strcmp(res[0].req, "ERR") == 0)
			message = "Error storing message on server";
		else if (strcmp(res[0].req, "TAL") == 0)
			message = "Topic already exists on server";
		else
			message = "Unknown error occured!!";
		
		return -1;
	}

public:
	Publisher(int socket) : socket{socket}
	{
		
	}

	int createTopic(string topic, string &errMsg)
	{
		if (topics.addTopic(topic, errMsg) == -1)
			return -1;

		bool isConnectionClosed;
		int snd_result = SocketIO::writeClientData(socket, "CRE", topic.c_str(), {}, errMsg, isConnectionClosed);

        if (isConnectionClosed)
		{
			cout << "Connection closed from server side!!" << endl;
			exit(-1);
		}

		// write error
		if (snd_result == -1)
			return -1;

		
		return getServerResponse(errMsg);
	}

	int sendMessage(const string& topic, const string& msg, string &errMsg)
	{
		cout << "Sending Topic: " << topic << " and msg: " << msg << endl;
		if (msg.size() > maxMessageSize)
		{
			errMsg = "Message length should be less than " + to_string(maxMessageSize + 1) + " characters";
			return -1;
		}

		bool isConnectionClosed;
		int snd_result = SocketIO::writeClientData(socket, "PUS", topic.c_str(), {msg}, errMsg, isConnectionClosed);

        if (isConnectionClosed)
		{
			cout << "Connection closed from server side!!" << endl;
			exit(-1);
		}

		// write error
		if (snd_result == -1)
			return -1;

		return getServerResponse(errMsg);
	}

	int sendFile(const string &topic, ifstream &inf, string &errMsg)
	{
		vector<string> msgs;
		while (inf)
		{
			string line;
			getline(inf >> ws, line);

			if (line.size() > maxMessageSize)
			{
				errMsg = "Message length should be less than " + to_string(maxMessageSize + 1) + " characters";
				return -1;
			}

			if (line != "")
				msgs.push_back(line);
		}
		
		bool isConnectionClosed;
		int snd_result = SocketIO::writeClientData(socket, "FPU", topic.c_str(), msgs, errMsg, isConnectionClosed);

        if (isConnectionClosed)
		{
			cout << "Connection closed from server side!!" << endl;
			exit(-1);
		}

		// write error
		if (snd_result == -1)
			return -1;

		return getServerResponse(errMsg);
	}
};

void do_task(int sockfd)
{
	Publisher p(sockfd);

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
		cout << "*********************Producer Panel*********************" << endl;
		cout << "0.\tExit" << endl;
		cout << "1.\tCreate a Topic" << endl;
		cout << "2.\tSend a message" << endl;
		cout << "3.\tSend messages from file" << endl;
		cout << "Select an option: ";
		cin >> option;

		if (option < 0 || option > 3)
			continue;

		if (option == 0)
			break;

		// clear the buffer and clear screen
		CLEAR_INPUT;
		clear_screen();

		string topic;
		cout << "Enter the topic name: ";
		std::getline(std::cin >> std::ws, topic);
		// convert to lowercase
		transform(topic.begin(), topic.end(), topic.begin(), ::tolower);
		
		// perform the tasks
		if (option == 1)
		{
			string errMsg;
			if (p.createTopic(topic, errMsg) == -1)
				cout << errMsg << endl;
			else
				cout << "Successfully added topic to server" << endl;
		}

		if (option == 2)
		{
			string message;
			
			cout << "Enter the message: ";
			std::getline(std::cin >> std::ws, message);

			string errMsg;
			if (p.sendMessage(topic, message, errMsg) == -1)
				cout << errMsg << endl;
			else
				cout << "Successfully added topic to server" << endl;
		}

		if (option == 3)
		{
			string file_path;
			cout << "Enter file path: ";
			std::getline(std::cin >> std::ws, file_path);

			ifstream inf {file_path};

			if (!inf)
			{
				cerr << "file open error" << endl;
				continue;
			}

			string errMsg;
			if (p.sendFile(topic, inf, errMsg) == -1)
				cout << errMsg << endl;
			else
				cout << "Successfully added topic to server" << endl;
		}

	}
}

int main(int argc, char** argv)
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

	// register as subscriber on server
	char buff[4];
	strcpy(buff, "PUB");
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
