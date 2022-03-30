#include <iostream>
#include <unordered_map>
#include <queue>
#include <vector>
#include <cassert>
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
#include <arpa/inet.h>
#include "helpers/SocketIO.h"
#include "helpers/Database.h"
#include "helpers/ThreadPool.h"
#include "helpers/Queue.h"

using namespace std;

struct ServerInfo
{
    const Queue<int> *q;
    ThreadPool thread_pool;

    const int PORT, right_port;
    SocketInfo listenSockInfo;
    int neighbor_fds[2];

    ServerInfo(const Queue<int> *q, int port, int right) : 
        q{q}, 
        PORT { port }, 
        right_port {right}, 
        thread_pool(MAX_CONNECTION_COUNT)
    {

    }
} *info;

namespace RequestHandler
{
    void createTopic(const vector<ClientMessage> &data, int connfd, string& errMsg, bool &isConnectionClosed)
    {
        assert(data.size() == 1);
        int status = Database::getInstance().addTopic(data[0].topic);
        string res = (status == -1) ? "TAL" : "OK";

        SocketIO::client_writeData(connfd, res.c_str(), data[0].topic, {}, errMsg, isConnectionClosed);
    }

    void pushMessage(const vector<ClientMessage> &data, int connfd, string& errMsg, bool &isConnectionClosed)
    {
        assert(data.size() == 1);
        int status = Database::getInstance().addMessage(data[0].topic, data[0].msg);
        string res = (status == -1) ? "NTO" : "OK";
        SocketIO::client_writeData(connfd, res.c_str(), data[0].topic, {}, errMsg, isConnectionClosed);
    }

    void pushFileContents(const vector<ClientMessage> &data, int connfd, string& errMsg, bool &isConnectionClosed)
    {
        vector<string> msgs;
        for (auto &msg: data)
            msgs.push_back(msg.msg);

        int status = Database::getInstance().addMessages(data[0].topic, msgs);
        string res = (status == -1) ? "NTO" : "OK";
        SocketIO::client_writeData(connfd, res.c_str(), data[0].topic, {}, errMsg, isConnectionClosed);
    }

    void getAllTopics(int connfd, string& errMsg, bool &isConnectionClosed)
    {
        vector<string> topics = Database::getInstance().getAllTopics();
        string res = topics.size() == 0 ? "NTO" : "OK";
        SocketIO::client_writeData(connfd, res.c_str(), "", topics, errMsg, isConnectionClosed);
    }

    void getNextMessage(const vector<ClientMessage> &data, int connfd, string& errMsg, bool &isConnectionClosed)
    {
        if (!Database::getInstance().topicExists(data[0].topic))
        {
            string res = "NTO";
            SocketIO::client_writeData(connfd, res.c_str(), data[0].topic, {}, errMsg, isConnectionClosed);
            return;
        }

        auto time = data[0].time;
        string msg = Database::getInstance().getNextMessage(data[0].topic, time);
        string res = msg == "" ? "NMG" : "OK";
        SocketIO::client_writeData(connfd, res.c_str(), data[0].topic, {msg}, errMsg, isConnectionClosed, time);
    }

    void getAllMessages(const vector<ClientMessage> &data, int connfd, string& errMsg, bool &isConnectionClosed)
    {
        if (!Database::getInstance().topicExists(data[0].topic))
        {
            string res = "NTO";
            SocketIO::client_writeData(connfd, res.c_str(), data[0].topic, {}, errMsg, isConnectionClosed);
            return;
        }

        auto time = data.back().time;
        auto msgs = Database::getInstance().getBulkMessages(data[0].topic, time);
        string res = msgs.size() == 0 ? "NMG" : "OK";
        SocketIO::client_writeData(connfd, res.c_str(), data[0].topic, msgs, errMsg, isConnectionClosed, time);
    }
}

void ClientHandler(const SocketInfo& sockinfo)
{
    while (true)
    {
        // wait to get data
        string errMsg;
        bool isConnectionClosed;
        auto data = SocketIO::client_readData(sockinfo.connfd, errMsg, isConnectionClosed);

        if (isConnectionClosed)
            break;

        if (data.size() == 0)   // error
        {
            cout << errMsg << endl;
            continue;
        }

        if (strcmp(data[0].req, "CRE") == 0)
            RequestHandler::createTopic(data, sockinfo.connfd, errMsg, isConnectionClosed);
        else if (strcmp(data[0].req, "PUS") == 0)
            RequestHandler::pushMessage(data, sockinfo.connfd, errMsg, isConnectionClosed);
        else if (strcmp(data[0].req, "FPU") == 0)
            RequestHandler::pushFileContents(data, sockinfo.connfd, errMsg, isConnectionClosed);
        else if (strcmp(data[0].req, "GAT") == 0)
            RequestHandler::getAllTopics(sockinfo.connfd, errMsg, isConnectionClosed);
        else if (strcmp(data[0].req, "GNM") == 0)
            RequestHandler::getNextMessage(data, sockinfo.connfd, errMsg, isConnectionClosed);
        else if (strcmp(data[0].req, "GAM") == 0)
            RequestHandler::getAllMessages(data, sockinfo.connfd, errMsg, isConnectionClosed);
        else
            SocketIO::client_writeData(sockinfo.connfd, "ERR", "", {}, errMsg, isConnectionClosed);

        if (errMsg == "")
            continue;

        if (isConnectionClosed)
            break;

        cout << errMsg << endl;
    }
}


void ServerConnection(int connfd)
{

}

void EstablishClient(int connfd)
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
    
    if (strcmp(BUFF, "PUB") == 0 || strcmp(BUFF, "SUB") == 0)
    {
        strcpy(BUFF, "OK");
        write(connfd, BUFF, sizeof(BUFF));
        ClientHandler(sockinfo);
    }
    else
    {
        strcpy(BUFF, "ERR");
        write(connfd, BUFF, sizeof(BUFF));
        cout << "Unknown client!!" << endl;
    }

    close(connfd);
    cout << ntohs(sockinfo.my_addr.sin_port)  << ": Connection closed from client " << inet_ntoa(sockinfo.dest_addr.sin_addr) << ":" << ntohs(sockinfo.dest_addr.sin_port) << endl;
}

void serverOnLoad()
{
    // Register SIGCHILD handler as lambda    
	struct sigaction act, oldact;
	act.sa_handler = [](int signo) -> void 
    {
	    pid_t pid;
	    int stat;

	    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0);
    };
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

    // Create a socket to receive the client connections
    info->listenSockInfo = SocketIO::makePassiveSocket(info->PORT);

    // synchronize with creator
    info->q->send_data(getppid(), getpid(), "msgsnd error");
    info->q->receive_data(getpid(), "msgrcv error");

    // asynchronously wait for connection
    pthread_t thread1, thread2;
    
    // wait for a connection on another thread
    pthread_create(&thread1, nullptr, 
        [](void* data) -> void* 
        {
            SocketInfo* info = (SocketInfo*)data;
            socklen_t addrlen = sizeof(info->dest_addr);

            while ((info->connfd = accept(info->sockfd, (struct sockaddr*)&(info->dest_addr), &addrlen)) < 0)
                if (errno == EINTR)
                    continue;
                else
                    perror("accept error");

            return nullptr;
        }, (void*)&info->listenSockInfo);

    // make a new connection request
    auto rightSockInfo = SocketIO::activeConnect("127.0.0.1", info->right_port);
    pthread_join(thread1, nullptr);

    cout << ntohs(info->listenSockInfo.my_addr.sin_port) << " : Connected to port " << ntohs(info->listenSockInfo.dest_addr.sin_port) << endl;
    cout << info->PORT << " : Connected to port " << ntohs(rightSockInfo.dest_addr.sin_port) << endl;

    // assign thread to neighbours
    void ServerConnection(int connfd);
    info->neighbor_fds[0] = info->listenSockInfo.connfd;
    info->neighbor_fds[1] = rightSockInfo.connfd;
    info->thread_pool.startOperation(info->neighbor_fds[0], ServerConnection);
    info->thread_pool.startOperation(info->neighbor_fds[1], ServerConnection);

    // synchronize with creator
    info->q->send_data(getppid(), getpid(), "msgsnd error");
    info->q->receive_data(getpid(), "msgrcv error");

    // Now, we listen for connection from clients
    while (true)
    {
        pthread_t thread;
        socklen_t addrlen = sizeof(info->listenSockInfo.dest_addr);

        while ((info->listenSockInfo.connfd = accept(info->listenSockInfo.sockfd, (struct sockaddr*)&info->listenSockInfo.dest_addr, &addrlen)) < 0)
            if (errno == EINTR)
                continue;
            else
                perror("accept error");

        info->thread_pool.startOperation(info->listenSockInfo.connfd, EstablishClient);
    }
}

int main(int argc, char** argv)
{
    const Queue<int> queue { ftok(".", 'b') };
    int start_port;
    int count;

    // Take input
    if (argc != 3)
    {
        printf("usage: server.o <N> <start port>\n");
        exit(1);
    }

    count = atoi(argv[1]);
    start_port = atoi(argv[2]);

    // Spawn servers
    pid_t *pids = new pid_t[count];

    for (int i = 0; i < count; ++i)
    {
        pid_t pid;
        if ((pid = fork()) == 0)
        {
            info = new ServerInfo { &queue, start_port + i, start_port + (i + 1) % count };
            freopen(("logs/" + to_string(info->PORT) + ".log").c_str(), "w", stderr);
            serverOnLoad();
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


    // Wait for all servers to finish
    for (int i = 0; i < count; ++i)
        wait(NULL);

    return 0;
}