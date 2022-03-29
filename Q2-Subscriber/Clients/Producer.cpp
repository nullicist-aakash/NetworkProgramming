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

using namespace std;

#define CLEAR_INPUT { char c; while ((c = getchar()) != '\n' && c != '\r' && c != EOF); }
#define PAUSE { printf("Press any key to continue...\n"); char c; scanf("%c", &c); }

const int maxMessageSize = 512;	
const int maxTopicSize = 20;

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

	bool topicExists(const string& topic) const
	{
		auto tmp = list;

		while (tmp != nullptr)
		{
			if (topic == tmp->topic)
				return true;
			
			tmp = tmp->next;
		}

		return false;
	}

	static bool isValidTopic(const string& topic)
	{
		return topic.size() <= maxTopicSize;
	}

	int addTopic(const string& topic)
	{
		if (!isValidTopic(topic))
		{
			cout << "Topic '" << topic << "' length should be less than " << maxTopicSize + 1 << " characters" << endl;
			return -1;
		}

		if (topicExists(topic))
		{
			cout << "Topic already exists!!" << endl;
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

class Producer
{
private:
	struct Message
	{
		char req[4];
		bool isLastData;
		clock_t time;
		int size;
		char topic[maxTopicSize + 1];
		char msg[maxMessageSize + 1];
	};

	const int socket;
	Topics topics;

	int getServerResponse()
	{
		Message m;
		int n = read(socket, (char*)&m, sizeof(m) - sizeof(m.msg));
		if (n < 0)
		{
			perror("reply read error");
			return -1;
		}

		if (n == 0)
		{
			cout << "Connection ended prematurely" << endl;
			return -1;
		}

		if (strcmp(m.req, "OK") == 0)
		{
			cout << "Success!!" << endl;
			return 0;
		}
		
		if (strcmp(m.req, "NTO") == 0)
			cout << "Topic doesn't exist on server!! Create topic first" << endl;
		else if (strcmp(m.req, "ERR") == 0)
			cout << "Error storing message on server" << endl;
		else if (strcmp(m.req, "TAL") == 0)
			cout << "Topic already exists on server" << endl;
		else
			cout << "Unknown error occured!!" << endl;
		
		return -1;
	}

	int sendDataToServer(Message& m, int size)
	{
		if (write(socket, (char*)&m, size) <= 0)
		{
			perror("write to server");
			return -1;
		}

		return 0;
	}

public:
	Producer(int socket) : socket{socket}
	{
		
	}

	int createTopic(string topic)
	{
		if (topics.topicExists(topic))	// use already stored error message
			return topics.addTopic(topic);

		Message m;
		m.isLastData = true;
		strcpy(m.req, "CRE");
		strcpy(m.topic, topic.c_str());

		if (sendDataToServer(m, sizeof(m) - sizeof(m.msg)) < 0)
			return -1;
		
		if (getServerResponse() == 0)
		{
			topics.addTopic(topic);
			return 0;
		}
		else
			return -1;
	}

	int sendMessage(const string& topic, const string& msg)
	{
		if (!topics.topicExists(topic))
		{
			cout << "Topic " << topic << " doesn't exist! Add it first" << endl;
			return -1;
		}

		if (msg.size() > maxMessageSize)
		{
			cout << "Message length should be less than " << maxMessageSize + 1 << " characters" << endl;
			return -1;
		}

		Message m;
		m.isLastData = true;
		strcpy(m.req, "PUS");
		strcpy(m.topic, topic.c_str());
		strcpy(m.msg, msg.c_str());

		if (sendDataToServer(m, sizeof(m)) < 0)
			return -1;

		return getServerResponse();
	}

	int sendFile(const string &topic, ifstream &inf)
	{
		if (!topics.topicExists(topic))
		{
			cout << "Topic " << topic << " doesn't exist! Add it first" << endl;
			return -1;
		}

		Message m;
		strcpy(m.req, "PUS");
		strcpy(m.topic, topic.c_str());
		
		while (inf)
		{
			string line;
			getline(inf, line);

			// cut the message to max size possible	
			if (line.size() > maxMessageSize)
				line.erase(maxMessageSize, std::string::npos);

			strcpy(m.msg, line.c_str());

			if (inf)
				m.isLastData = false;
			else
				m.isLastData = true;

			if (sendDataToServer(m, sizeof(m)) < 0)
				return -1;
		}

		return getServerResponse();
	}

	bool topicExists(const string &topic) const
	{
		return topics.topicExists(topic);
	}
};

void do_task(int sockfd)
{
	Producer p(sockfd);

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
			p.createTopic(topic);

		if (option == 2)
		{
			if (!p.topicExists(topic))
			{
				cout << "Topic '" << topic << "' doesn't exist" << endl;
				continue;
			}

			string message;
			
			cout << "Enter the message: ";
			std::getline(std::cin >> std::ws, message);

			p.sendMessage(topic, message);
		}

		if (option == 3)
		{
			if (!p.topicExists(topic))
			{
				cout << "Topic '" << topic << "' doesn't exist" << endl;
				continue;
			}

			string file_path;
			cout << "Enter file path: ";
			std::getline(std::cin >> std::ws, file_path);

			ifstream inf {file_path};

			if (!inf)
			{
				cerr << "file open error" << endl;
				continue;
			}

			p.sendFile(topic, inf);
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
