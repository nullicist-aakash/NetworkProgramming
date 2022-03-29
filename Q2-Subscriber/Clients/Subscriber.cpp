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

class Subscriber
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
		else if (strcmp(m.req, "NMG") == 0)
			cout << "No more messages on server" << endl;
		else
			cout << "Unknown error '" << m.req << "' occured!!" << endl;
		
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
	Subscriber(int socket) : socket{socket}
	{
		
	}
	
	bool topicExists(const string &topic) const
	{
		return topics.topicExists(topic);
	}
};

void do_task(int sockfd)
{
	Subscriber p(sockfd);

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

		string topic;
		cout << "Enter the topic name: ";
		std::getline(std::cin >> std::ws, topic);
		// convert to lowercase
		transform(topic.begin(), topic.end(), topic.begin(), ::tolower);
		
		// perform the tasks
		if (option == 1)
		{

		}

		if (option == 2)
		{

		}

		if (option == 3)
		{
			
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

	do_task(sockfd);
	close(sockfd);

	return 0;
}
