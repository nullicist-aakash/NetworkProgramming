#include <iostream>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <arpa/inet.h>
using namespace std;

#define MAX_CONNECTION_COUNT 32

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

template <class T>
class Queue
{
private:
    int qid;

    struct my_msg
    {
        long mtype;
        T data;
    };

public:
    Queue(key_t id)
    {
        if (id == -1)
        {
            perror("ftok");
            exit(-1);
        }

        qid = msgget(id, IPC_CREAT | 0660);

        if (qid == -1)
        {
            perror("msgget");
            exit(-1);
        }
    }

    void send_data(int type, const T data, const char* errMsg = nullptr) const
    {
        my_msg msg { type, data };

        if (msgsnd(qid, &msg, sizeof(msg) - sizeof(long), 0) == -1)
        {
            perror(errMsg == nullptr ? "msgsnd" : errMsg);
            exit(-1);
        }
    }

    T receive_data(int type, const char* errMsg = nullptr) const
    {
        my_msg msg;
        
        if (msgrcv(qid, &msg, sizeof(my_msg) - sizeof(long), type, 0) == -1)
        {
            perror(errMsg == nullptr ? "msgrcv" : errMsg);
            exit(-1);
        }

        return msg.data;
    }

    ~Queue()
    {
        if (msgctl(qid, IPC_RMID, nullptr) == -1)
                perror("msgctl");
    }
};

struct SocketInfo
{
    int sockfd;
    int connfd;
    struct sockaddr_in my_addr;
    struct sockaddr_in dest_addr;
};

template <class T>
struct LinkedListNode
{
    const T data;
    LinkedListNode<T>* next;
    LinkedListNode<T>* prev;

    LinkedListNode(T &data, ConcurrentLinkedListNode<T>* prev = nullptr, ConcurrentLinkedListNode<T>* next = nullptr) 
        : data {data}, prev{prev}, next { next }
    {

    }
};

template <class T>
class ConcurrentLinkedList
{
private:
    LinkedListNode<T> *head, *tail;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

public:
    ConcurrentLinkedList()
    {
        head = tail = nullptr;
    }

    void insert(T &data)
    {
        pthread_mutex_lock(&mutex);
        auto node = new LinkedListNode<T>(data);

        if (head == nullptr)
            head = tail = node;
        else
        {
            tail->next = node;
            tail = node;
        }

        pthread_mutex_unlock(&mutex);
    }

    void remove(LinkedListNode<T>* node)
    {
        pthread_mutex_lock(&mutex);
        if (node == head && node == tail)
        {
            delete node;
            head = tail = nullptr;
        }
        else if (node == head)
        {
            head = head->next;
            delete node;
            head->prev = nullptr;
        }
        else if (node == tail)
        {
            tail = tail->prev;
            delete node;
            tail->next = nullptr;
        }
        else
        {
            node->prev->next = node->next;
            node->next->prev = node->prev;
            delete node;
        }
        pthread_mutex_unlock(&mutex);
    }

    ~ConcurrentLinkedList()
    {
        pthread_mutex_lock(&mutex);

        if (head == nullptr)
        {
            pthread_mutex_unlock(&mutex);
            return;
        }

        while (head)
        {
            auto temp = head;
            delete head;
            head = temp;
        }

        tail = nullptr;
        pthread_mutex_unlock(&mutex);
    }
};

template <class T>
class ConcurrentCircQueue
{
private:
    const int count;
    T *arr;
    int front;
    int rear;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

public:
    ConcurrentCircQueue(int n) : count {n}
    {
        arr = new T[n];
        front = -1;
        rear = -1;
    }

    int enqueue(T &data)
    {
        pthread_mutex_lock(&mutex);
        if (front == -1 && rear == -1)
        {
            front = rear = 0;
            arr[rear] = data;
        }
        else if ((rear + 1) % count != front)
        {
            rear++;
            rear %= count;
            queue[rear] = data;
        }
        else
        {        
            pthread_mutex_unlock(&mutex);
            return -1;
        }

        pthread_mutex_unlock(&mutex);
        return 0;
    }
    
    T deque()
    {

    }
};

template <class T>
class ThreadPool
{
private:
    const int nthreads;
    T *clifd;
    pthread_t *threads;
    int iget, iput;

    const std::function<void(T)> lambda;

    pthread_mutex_t clifd_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t clifd_cond = PTHREAD_COND_INITIALIZER;

    static void* thread_main(void* arg)
    {
        ThreadPool* pool = (ThreadPool*)arg;
        
        while (1)
        {
            pthread_mutex_lock(&pool->clifd_mutex);

            while (pool->iget == pool->iput)
                pthread_cond_wait(&pool->clifd_cond, &pool->clifd_mutex);
            
            if (++pool->iget == pool->nthreads)
                pool->iget = 0;

            pthread_mutex_unlock(&pool->clifd_mutex);

            pool->lambda(pool->clifd[pool->iget]);
            close(connfd);
        }
    }

public:
    ThreadPool(int num_threads, std::function<void(int)>& lambda) : nthreads {num_threads}, lambda {lambda}
    {
        iget = iput = 0;
        threads = new pthread_t[num_threads];
        clifd = new int[num_threads];
        for (int i = 0; i < num_threads; ++i)
            pthread_create(&threads[i], NULL, &thread_main, (void*)this);
    }

    void startOperation(T &connfd)
    {
        pthread_mutex_lock(&clifd_mutex);
        clifd[iput] = connfd;

        if (++iput == nthreads)
            iput = 0;
        
        if (iput == iget)
        {
            cout << "iput = iget = " << iput << endl;
            exit(1);
        }

        pthread_cond_signal(&clifd_cond);
        pthread_mutex_unlock(&clifd_mutex);
    }

    ~ThreadPool()
    {
        delete[] clifd;
        delete[] threads;
    }
};

class Server
{
private:
    const Queue<int> *q;

    const int PORT, right_port;
    SocketInfo listenSockInfo;
    int neighbor_fds[2];

    SocketInfo makePassiveSocket()
    {
        SocketInfo info;

        // make socket
        if ((info.sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        {
            perror("socker error");
            exit(1);
        }

        // fill own port details
        memset(&info.my_addr, 0, sizeof(info.my_addr));
	    info.my_addr.sin_family = AF_INET;
	    info.my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    	info.my_addr.sin_port = htons(PORT);

        // attach PORT to socket
        if (bind(info.sockfd, (struct sockaddr*)&info.my_addr, sizeof(info.my_addr)) < 0)
        {
            perror("bind error");
            exit(1);
        }

        cout << "Listening to PORT " << ntohs(info.my_addr.sin_port) << "..." << endl;

        //  move from CLOSED to LISTEN state, create passive socket
        if (listen(info.sockfd, MAX_CONNECTION_COUNT) < 0)
        {
            perror("listen error");
            exit(1);
        }

        return info;
    }

    static SocketInfo activeConnect(const char* ip, int PORT)
    {
        SocketInfo info;
        info.sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (info.sockfd < 0)
        {
            perror("socket error");
            exit(1);
        }

        memset(&info.dest_addr, 0, sizeof(info.dest_addr));
        info.dest_addr.sin_family = AF_INET;
        info.dest_addr.sin_port = htons(PORT);
        info.dest_addr.sin_addr.s_addr = inet_addr(ip);

        if (connect(info.sockfd, (struct sockaddr*)&info.dest_addr, sizeof(info.dest_addr)) < 0)
        {
            perror("connect error");
            exit(1);
        }

        info.connfd = info.sockfd;
        return info;
    }

    // made static so that pthread can call it
    static void* waitForPassiveConnection(void* data)
    {
        auto *info = (SocketInfo*)data;
        socklen_t addrlen = sizeof(info->dest_addr);

        while ((info->connfd = accept(info->sockfd, (struct sockaddr*)&(info->dest_addr), &addrlen)) < 0)
            if (errno == EINTR)
                continue;
            else
                perror("accept error");

        return nullptr;
    }

    static void* NeighborConnection(void* data)
    {
        int connfd = *(int*)data;

        return nullptr;
    }

    void ClientConnection(int connfd, sockaddr_in dest_addr)
    {
        cout << "Server with PORT " << PORT << " is connected to client " << inet_ntoa(dest_addr.sin_addr) << endl;
    }

public:
    Server(const Queue<int> *q, int port, int right) : q{q}, PORT { port }, right_port {right}
    {

    }

    void serverOnLoad()
    {
	    Signal(SIGCHLD, sig_child);
        listenSockInfo = makePassiveSocket();

        // synchronize with creator
        q->send_data(getppid(), getpid(), "msgsnd error");
        q->receive_data(getpid(), "msgrcv error");

        // asynchronously wait for connection
        pthread_t thread1, thread2;
        pthread_create(&thread1, nullptr, &waitForPassiveConnection, (void*)&listenSockInfo);

        // make a new connection request
        auto rightSockInfo = activeConnect("127.0.0.1", right_port);
        pthread_join(thread1, nullptr);

        cout << ntohs(listenSockInfo.my_addr.sin_port) << " : Connected to port " << ntohs(listenSockInfo.dest_addr.sin_port) << endl;
        cout << PORT << " : Connected to port " << ntohs(rightSockInfo.dest_addr.sin_port) << endl;

        // assign thread to neighbours
        neighbor_fds[0] = listenSockInfo.connfd;
        neighbor_fds[1] = rightSockInfo.connfd;
        pthread_create(&thread1, nullptr, &NeighborConnection, neighbor_fds);
        pthread_create(&thread2, nullptr, &NeighborConnection, neighbor_fds + 1);

        // synchronize with creator
        q->send_data(getppid(), getpid(), "msgsnd error");
        q->receive_data(getpid(), "msgrcv error");

        // Now, we listen for connection from clients
        while (true)
        {
            pthread_t thread;
            pthread_create(&thread, nullptr, &waitForPassiveConnection, (void*)&listenSockInfo);
            pthread_join(thread, nullptr);

            // serve the client
            if (fork() == 0)
            {
                close(listenSockInfo.sockfd);
                ClientConnection(listenSockInfo.connfd, listenSockInfo.dest_addr);
                exit(0);
            }

            close(listenSockInfo.connfd);
        }
    }
};

class ServerInitialiser
{
private:
    const Queue<int> queue;

    int start_port;
    int count;
    Server** servers;

public:
    ServerInitialiser(int argc, char** argv) : queue { ftok(".", 'b') }
    {
        if (argc != 3)
        {
            printf("usage: server.o <N> <start port>\n");
            exit(1);
        }

        count = atoi(argv[1]);
        start_port = atoi(argv[2]);
    }

    void spawnServers()
    {
        pid_t *pids = new pid_t[count];
        servers = new Server*[count];

        for (int i = 0; i < count; ++i)
        {
            pid_t pid;
            if ((pid = fork()) == 0)
            {
                servers[i] = new Server { &queue, start_port + i, start_port + (i + 1) % count };
                servers[i]->serverOnLoad();
                delete servers[i];
                exit(0);
            }
            else if (pid == -1)
            {
                perror("fork");
                exit(-1);
            }
        }
  
        // wait for all children to send the connection success message
        for (int i = 0; i < count; ++i)
            pids[i] = queue.receive_data(getpid(), "msgrcv error from initialiser");
        
        // send the signal to all children to start making connectiions
        for (int i = 0; i < count; ++i)
            queue.send_data(pids[i], 0, "msgsnd error from initialiser");
            
        // wait for all children to send the connection success message
        for (int i = 0; i < count; ++i)
            pids[i] = queue.receive_data(getpid(), "msgrcv error from initialiser");
        
        // send the signal to all children to start making connectiions
        for (int i = 0; i < count; ++i)
            queue.send_data(pids[i], 0, "msgsnd error from initialiser");

        delete[] pids;
    }

    ~ServerInitialiser()
    {
        for (int i = 0; i < count; ++i)
            wait(NULL);

        delete[] servers;
    }
};

int main(int argc, char** argv)
{
    ServerInitialiser initializer(argc, argv);

    // make n servers in memory
    initializer.spawnServers();

    // initialiser will wait for servers to be closed during destruction, so this process will never terminate
    return 0;
}