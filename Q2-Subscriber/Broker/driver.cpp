#include <iostream>
#include <functional>
#include <unordered_map>
#include <queue>
#include <vector>
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
#define BULK_LIMIT 10
#define MESSAGE_TIME_LIMIT 60

const int maxMessageSize = 512;	
const int maxTopicSize = 20;

struct SocketInfo
{
    int sockfd;
    int connfd;
    struct sockaddr_in my_addr;
    struct sockaddr_in dest_addr;
};
  
struct ClientMessage
{
    char req[4];
    bool isLastData;
    clock_t time;
    int cur_size;
    char topic[maxTopicSize + 1];
    char msg[maxMessageSize + 1];
};

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

class Server;

template <class T>
class ThreadPool
{
private:
    const int nthreads;
    Server* server;
    T *clifd;
    function<void(Server*, T)> *lambdas;

    pthread_t *threads;
    int iget, iput;

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
            
            int index = pool->iget;

            if (++pool->iget == pool->nthreads)
                pool->iget = 0;

            pthread_mutex_unlock(&pool->clifd_mutex);

            int connfd = pool->clifd[index];

            pool->lambdas[index](pool->server, connfd);
            close(connfd);
        }
    }

public:
    ThreadPool(int num_threads, Server* server) : nthreads {num_threads}, server {server}
    {
        iget = iput = 0;
        threads = new pthread_t[num_threads];
        clifd = new int[num_threads];
        lambdas = new function<void(Server*, T)>[num_threads];
        for (int i = 0; i < num_threads; ++i)
            lambdas[i] = nullptr;

        for (int i = 0; i < num_threads; ++i)
            pthread_create(&threads[i], NULL, &thread_main, (void*)this);
    }

    void startOperation(T &connfd, const std::function<void(Server*, int)> lambda)
    {
        pthread_mutex_lock(&clifd_mutex);
        clifd[iput] = connfd;
        lambdas[iput] = lambda;
        
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

class Database
{
private:
    using pcs = pair<clock_t, string>;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    unordered_map<string, priority_queue<pcs, vector<pcs>, greater<pcs>>> mp;

    inline void lock()
    {
        pthread_mutex_lock(&mutex);
    }

    inline void unlock()
    {
        pthread_mutex_unlock(&mutex);
    }

    void removeOldMessages(const string &topic, const clock_t &time)
    {
        lock();

        while (!mp[topic].empty() && ((time - mp[topic].top().first) / CLOCKS_PER_SEC) > MESSAGE_TIME_LIMIT)
            mp[topic].pop();

        unlock();
    }

public:
    int addTopic(const char* in_topic)
    {
        if (topicExists(in_topic))
        {
            cout << "Topic already exists!" << endl;
            return -1;
        }
        string topic(in_topic);

        lock();
        mp[topic] = {};
        unlock();
        return 0;
    }

    inline bool topicExists(const char* in_topic) const
    {
        string topic(in_topic);
        return mp.find(topic) != mp.end();
    }

    int addMessage(const char* in_topic, const char* in_message)
    {
        string topic(in_topic);
        string message(in_message);

        if (!topicExists(in_topic))
            return -1;

        clock_t time = clock();
        removeOldMessages(topic, time);

        lock();

        mp[topic].push({time, message});

        unlock();
        return 0;
    }

    const int getMessage(const char* in_topic, clock_t &clk, string &output)
    {
        string topic(in_topic);
        output = "";

        if (!topicExists(in_topic))
            return -1;

        clock_t time = clock();
        removeOldMessages(topic, time);

        lock();

        priority_queue<pcs> pq;
        while (!mp[topic].empty() && mp[topic].top().first < clk)
        {
            pq.push(mp[topic].top());
            mp[topic].pop();
        }

        if (!mp[topic].empty())
        {
            output = mp[topic].top().second;
            clk = mp[topic].top().first;
        }
        
        while (!pq.empty())
        {
            mp[topic].push(pq.top());
            pq.pop();
        }

        unlock();

        if (output == "")
            return -1;
        
        return 0;
    }

    const vector<string> getBulkMessages(const char* in_topic)
    {
        if (!topicExists(in_topic))
            return {};
        
        removeOldMessages(in_topic, clock());

        lock();
        string topic(in_topic);

        vector<pcs> temp;
        for (int i = 0; i < BULK_LIMIT; ++i)
        {
            if (mp[topic].empty())
                break;
            
            temp.push_back(mp[topic].top());
            mp[topic].pop();
        }

        vector<string> ans;
        for (auto &x: temp)
        {
            ans.push_back(x.second);
            mp[topic].push(x);
        }

        unlock();

        return ans;
    }

    const vector<string> getAllTopics() const
    {
        vector<string> ret;
        for (auto &[key, val]: mp)
            ret.push_back(key);

        return ret;
    }
};

class Server
{
private:
    const Queue<int> *q;
    ThreadPool<int> thread_pool;
    Database database;

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
        SocketInfo* info = (SocketInfo*)data;
        socklen_t addrlen = sizeof(info->dest_addr);

        while ((info->connfd = accept(info->sockfd, (struct sockaddr*)&(info->dest_addr), &addrlen)) < 0)
            if (errno == EINTR)
                continue;
            else
                perror("accept error");

        return nullptr;
    }

    static void ServerConnection(Server* server, int connfd)
    {
        int other_fd = connfd == server->neighbor_fds[0] ? server->neighbor_fds[1] : server->neighbor_fds[0];


    }

    static void SubscriberConnection(Server* server, const SocketInfo& sock)
    {

    }

    static void PublisherConnection(Server* server, const SocketInfo& sockinfo)
    {
        ClientMessage msg;
        int n = 1;

        while (n)
        {
            bool completeReceived = false;
            string res;
            
            while (!completeReceived && (n = read(sockinfo.connfd, (void*)&msg, sizeof(msg))) != 0)
            {
                if (n < 0)
                {
                    perror("read error");
                    cout << ntohs(sockinfo.my_addr.sin_port)  << ": Connection closed from client " << inet_ntoa(sockinfo.dest_addr.sin_addr) << ":" << ntohs(sockinfo.dest_addr.sin_port) << endl;
                    return;
                }

                completeReceived = msg.isLastData;
                cout << ntohs(sockinfo.my_addr.sin_port)  << ": Received { '" << msg.req << "', '" << msg.topic << "', '" << msg.msg << "' }" << endl;
                
                if (strcmp(msg.req, "CRE") == 0)
                {
                    int status = server->database.addTopic(msg.topic);

                    if (status == -1)
                        res = "TAL";
                    else
                        res = "OK";
                }

                if (strcmp(msg.req, "PUS") == 0)
                {
                    int status = server->database.addMessage(msg.topic, msg.msg);
                    cout << "Request status: " << status << endl;
                    if (status == -1)
                        res = "NTO";
                    else if (completeReceived)
                        res = "OK";
                }
            }

            msg.isLastData = true;
            if (completeReceived)
                strcpy(msg.req, res.c_str());
            else
                strcpy(msg.req, "ERR");
            
            int m;
            cout << ntohs(sockinfo.my_addr.sin_port)  << ": Sending { " << msg.req << " }" << endl;

            if ((m = write(sockinfo.connfd, (void*)&msg, sizeof(msg) - sizeof(msg.msg))) <= 0)
                perror("write error");
        }
    }

    int sendAllTopics(int connfd)
    {
        string s;
        for (auto &x: database.getAllTopics())
            s += x + "\n";

        const char* s_str = s.c_str();

        ClientMessage msg;
        strcpy(msg.topic, "");
        strcpy(msg.req, "OK");
        msg.time = clock();

        if (s.size() == 0)
        {
            strcpy(msg.req, "NMG");
            msg.isLastData = true;

            if (write(connfd, (void*)&msg, sizeof(msg)) <= 0)
            {
                perror("write error");
                return -1;
            }
        }

        int res_count = (s.size() / maxMessageSize) + (s.size() % maxMessageSize != 0);
        for (int i = 0; i < res_count; ++i)
        {
            while (s.size() > maxMessageSize)
            {
                int m;
                msg.cur_size = min(s.size() - (i * maxMessageSize), (unsigned long)maxMessageSize);

                memcpy((void*)msg.msg, (void*)(s_str + i * maxMessageSize), msg.cur_size);
                msg.msg[msg.cur_size] = '\0';
                msg.isLastData = i == (res_count - 1);

                if ((m = write(connfd, (void*)&msg, sizeof(msg))) <= 0)
                {
                    perror("write error");
                    return -1;
                }
            }
        }

        return 0;
    }

    static void ClientConnection(Server* server, int connfd)
    {
        int n = 1;
        SocketInfo sockinfo;
        sockinfo.connfd = connfd;
        socklen_t size = sizeof(sockinfo.dest_addr);

        if (getpeername(connfd, (struct sockaddr*)&sockinfo.dest_addr, &size) < 0)
        {
            perror("getpeername error");
            return;
        }

        if (getsockname(connfd, (struct sockaddr*)&sockinfo.my_addr, &size) < 0)
        {
            perror("getsockname error");
            return;
        }

        cout << ntohs(sockinfo.my_addr.sin_port)  << ": Connected to client " << inet_ntoa(sockinfo.dest_addr.sin_addr) << ":" << ntohs(sockinfo.dest_addr.sin_port) << endl;

        char BUFF[4];
        n = read(connfd, (void*)BUFF, sizeof(BUFF));
        if (n < 0)
        {
            perror("read error");
            cout << ntohs(sockinfo.my_addr.sin_port)  << ": Connection closed from client " << inet_ntoa(sockinfo.dest_addr.sin_addr) << ":" << ntohs(sockinfo.dest_addr.sin_port) << endl;
            return;
        }
        
        if (n == 0)
        {
            cout << ntohs(sockinfo.my_addr.sin_port)  << ": Connection closed from client " << inet_ntoa(sockinfo.dest_addr.sin_addr) << ":" << ntohs(sockinfo.dest_addr.sin_port) << endl;
            return;
        }

        if (strcmp(BUFF, "PUB") == 0)
        {
            strcpy(BUFF, "OK");
            write(connfd, BUFF, sizeof(BUFF));
            PublisherConnection(server, sockinfo);
        }
        else if (strcmp(BUFF, "SUB") == 0)
        {
            strcpy(BUFF, "OK");
            write(connfd, BUFF, sizeof(BUFF));
            SubscriberConnection(server, sockinfo);
        }
        else
        {
            strcpy(BUFF, "ERR");
            write(connfd, BUFF, sizeof(BUFF));
            cout << "Unknown client!!" << endl;
            close(connfd);
        }

        cout << ntohs(sockinfo.my_addr.sin_port)  << ": Connection closed from client " << inet_ntoa(sockinfo.dest_addr.sin_addr) << ":" << ntohs(sockinfo.dest_addr.sin_port) << endl;
    }

public:
    Server(const Queue<int> *q, int port, int right) : 
        q{q}, 
        PORT { port }, 
        right_port {right}, 
        thread_pool(32, this)
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
        thread_pool.startOperation(neighbor_fds[0], ServerConnection);
        thread_pool.startOperation(neighbor_fds[1], ServerConnection);

        // synchronize with creator
        q->send_data(getppid(), getpid(), "msgsnd error");
        q->receive_data(getpid(), "msgrcv error");

        // Now, we listen for connection from clients
        while (true)
        {
            pthread_t thread;
            socklen_t addrlen = sizeof(listenSockInfo.dest_addr);

            while ((listenSockInfo.connfd = accept(listenSockInfo.sockfd, (struct sockaddr*)&listenSockInfo.dest_addr, &addrlen)) < 0)
                if (errno == EINTR)
                    continue;
                else
                    perror("accept error");

            thread_pool.startOperation(listenSockInfo.connfd, ClientConnection);
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