#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <iostream>
#include <sstream>
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

class Subscriber
{
private:
	const int socket;
	string topic;

public:
	Subscriber(int socket) : socket{socket}
	{
		
	}
	
	void setTopic(const string &topic)
	{
		this->topic = topic;
	}

	bool isTopicSubscribed() const
	{
		return topic != "";
	}

	vector<string> getAllTopicsFromServer() const
	{
		Message m;
		strcpy(m.req, "GET");
		m.isLastData = true;

		int n = write(socket, (void*)&m, sizeof(m) - sizeof(m.msg));
		if (n <= 0)
		{
			perror("write error");
			return {};
		}

		string output;
		int status = getServerResponse(output);

		stringstream sstr(output);
		string topic;
		vector<string> out;

		while (sstr)
		{
			getline(sstr >> std::ws, topic);

			if (topic != "")
				out.push_back(topic);
			
			topic = "";
		}

		return out;
	}

	int getNextMessage(string &message)
	{
		Message m;
		strcpy(m.req, "GNE");
		
	}
};

void do_task(int sockfd)
{
	Subscriber p(sockfd);

	int option = -1;
	char c = 0;
	clock_t clk = -1;

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
			auto topics = p.getAllTopicsFromServer();

			if (topics.size() == 0)
				continue;

			cout << "Following are the topics on server..." << endl;
			cout << "0 : Go Back" << endl;
			for (int i = 0; i < topics.size(); ++i)
				cout << i + 1 << " : " << topics[i] << endl;

			cout << "Select topic number: ";
			int opt;
			cin >> opt;

			if (opt == 0)
				continue;

			if (opt < 0 || opt > topics.size())
			{
				cout << "Invalid option selected!" << endl;
				continue;
			}

			p.setTopic(topics[opt]);

			cout << "Topic selected: " << topics[opt] << endl;
		}

		if (option == 2)
		{
			if (!p.isTopicSubscribed())
			{
				cout << "First subscribe to a topic!!" << endl;
				continue;
			}
		}

		if (option == 3)
		{
			if (!p.isTopicSubscribed())
			{
				cout << "First subscribe to a topic!!" << endl;
				continue;
			}


		}
	}
}

int main(int argc, char** argv)
{
	int sockfd;
	struct sockaddr_in servaddr;

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
