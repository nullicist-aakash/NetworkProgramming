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
#include <sys/epoll.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>
#include <map>
#include <iostream>
using namespace std;

class Server
{
    const int PORT;
    const int N;
    int listenfd;
    sockaddr_in servaddr;

    int epfd;
    epoll_event *evlist;

    map<int, sockaddr_in*> fd_to_address;

public:
    Server(int port, int N) : PORT {port}, N {N}
    {
        listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (listenfd < 0)
        {
            perror("socket error");
            exit(-1);
        }

        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(port);
        
        if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
        {
            perror("bind error");
            exit(-1);
        }

        if (listen(listenfd, N) < 0)
        {
            perror("listen error");
            exit(-1);
        }

        // create fd for epoll
    
        epfd = epoll_create(N);
        evlist = new epoll_event[N];
        if (epfd == -1)
        {
            perror("epoll error");
            exit(-1);
        }

        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = listenfd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1)
        {
            perror("epoll_ctl error");
            exit(-1);
        }
    }

    void start_server_task()
    {
    // goto and labels are used becaue we need infinite loop with no break, and unnecessary nesting of paranthesis is not required if 'continue' is replaced by 'goto start'
    start:
        int ready = epoll_wait(epfd, evlist, N, -1);
        if (ready == -1)
        {
            if (errno == EINTR)
                goto start;
            
            perror("epoll_wait");
            exit(-1);
        }

        for (int j = 0; j < ready; ++j)
        {
            if (!(evlist[j].events & EPOLLIN))
                continue;

            if (evlist[j].data.fd == listenfd)
            {
                sockaddr_in *cliaddr = new sockaddr_in;
                socklen_t clilen = sizeof(cliaddr);
                int connfd = accept(listenfd, (struct sockaddr*)cliaddr, &clilen);

                epoll_event ev;
                ev.events = EPOLLIN;
                ev.data.fd = connfd;

                if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev) == -1)
                {
                    perror("epoll_ctl");
                    exit(-1);
                }

                fd_to_address[connfd] = cliaddr;
                continue;               
            }
            
            // client connection here
            int connfd = evlist[j].data.fd;
            int num = 0;
            int s = read(evlist[j].data.fd, &num, 4);
            
            if (s == -1)
            {
                perror("read");
                exit(-1);
            }

            if (s == 0)     // connection closed
            {
                close(connfd);
                delete fd_to_address[connfd];
                fd_to_address[connfd] = nullptr;
                continue;
            }

            char temp[32];
            cout << getpid() << " - Received message from - " << inet_ntop(AF_INET, &(fd_to_address[connfd]->sin_addr), temp, 32) << ":" << ntohs(fd_to_address[connfd]->sin_port) << endl;
            cout << "\t'" << num << "'" << endl;

            for (auto &[_connfd, address]: fd_to_address)
            {
                if (!address)
                    continue;

                if (((_connfd ^ connfd) & 1) == 0)
                    continue;
                
                // different parity
                cout << getpid() << " - Sending message to - " << inet_ntop(AF_INET, &(address->sin_addr), temp, 32) << ":" << ntohs(address->sin_port) << endl;
                cout << "\t'" << num << "'" << endl;

                int n = write(_connfd, &num, 4);
                if (n < 0)
                {
                    perror("write error");
                    exit(-1);
                }
            }
        }
        goto start;
    }
};

int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

void* snd_data(void* fd)
{
    int connfd = *(int*)fd;
    int port = *((int*)(fd) + 1);

    while (true)
    {
        int num = 0;
        msleep((rand() % 10000) + 100);

        num = rand();

        cout << getpid() << " - Sending message to parent." << endl;
        cout << "\t'" << num << "'" << endl;

        write(connfd, &num, 4);
    }
}

void client_handler(int index, int port)
{
    srand(index);
    
    // create socket in CLOSED state
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in servaddr;

	// fill the server details
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	// perform three way handshake
	if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		perror("connect error");
		exit(1);
	}

    int arr[2] { sockfd, port };

    pthread_t thread;
    pthread_create(&thread, nullptr, snd_data, (void*)arr);
    
    while (true)
    {
        int num;
        int n = read(sockfd, &num, 4);

        if (n < 0)
        {
            perror("read");
            exit(-1);
        }

        if (n == 0)
        {
            cout << getpid() << " - Connection closed!" << endl;
            exit(-1);
        }

        cout << getpid() << " - Received: " << num << endl;
    }
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		printf("usage: ./a.out <PORT> <N>\n");
		exit(1);
	}

    int port = atoi(argv[1]);
    int N = atoi(argv[2]);

    Server s(port, N);
    
    for (int i = 0; i < N; ++i)
    {
        if (fork() == 0)
        {
            client_handler(i, port);
            exit(0);
        }
    }

    s.start_server_task();
	return 0;
}
