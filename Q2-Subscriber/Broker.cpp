#include <iostream>
#include <unordered_map>
#include <queue>
#include <vector>
#include <set>
#include <map>
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
#include "helpers/Time.h"
#include "helpers/SocketLayer.h"
#include "helpers/Database.h"
#include "helpers/ThreadPool.h"
#include "helpers/Queue.h"

using namespace std;

struct ServerInfo
{
    const Queue<int> *q;
    ThreadPool thread_pool;
    pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;

    const int PORT, server_send_port, server_recv_port;
    SocketInfo sendServerPORTInfo;
    SocketInfo listenSockInfo;

    ServerInfo(const Queue<int> *q, int port, int send, int recv) : 
        q{q}, 
        PORT { port }, 
        server_send_port {send},
        server_recv_port {recv}, 
        thread_pool(MAX_CONNECTION_COUNT + 1)
    {

    }
} *info;

unordered_map<int, vector<ClientPayload>> serv_response;

void accumulateResponses(int threadid, const vector<ClientPayload> &send_data)
{
    // prepare request for neighboring server
    vector<ServerPayload> serv_payload;
    for (auto &x: send_data)
    {
        ServerPayload p;
        p.sender_server_port = info->PORT;
        p.sender_thread_id = threadid;

        memcpy(&p.client_payload, &x, sizeof(ClientPayload));
        serv_payload.push_back(p);
    }

    cerr << "Locking info->send_mutex" << endl;
    // send request to neighboring server
    pthread_mutex_lock(&info->send_mutex);
    PresentationLayer::sendServerData(info->sendServerPORTInfo.connfd, serv_payload);
    cerr << "Unlocking info->send_mutex" << endl;
    pthread_mutex_unlock(&info->send_mutex);

    // wait for the receive function to release the lock
    cerr << "Waiting on thread mutex" << endl;
    pthread_cond_wait(
        &info->thread_pool.getCondFromThreadID(threadid), 
        &info->thread_pool.getMutexFromThreadID(threadid));
    cerr << getpid() << "\tSignaling receive mutex" << endl;
}

// On access validation from a client
void clientHandler(const SocketInfo& sockinfo, int threadid)
{
    Database &database = Database::getInstance();

    string errMsg;
    bool isConnectionClosed = false;

    while (true)
    {
        // instead of handling the send errors at end of loop, it is handled here because of continue statements in program
        if (errMsg != "")
        {
            std::cout << errMsg << endl;
            break;
        }
        
        auto data = PresentationLayer::getData(sockinfo.connfd, errMsg, isConnectionClosed);
        if (errMsg != "")
        {
            std::cout << errMsg << endl;
            break;
        }

        if (data[0].msgType == MessageType::CREATE_TOPIC)
        {
            ClientPayload payload;
            memset(&payload, 0, sizeof(payload));
            payload.time = current_time();

            if (database.addTopic(data[0].topic) == -1)
                payload.msgType = MessageType::TOPIC_ALREADY_EXISTS;
            else
                payload.msgType = MessageType::SUCCESS;
            
            strcpy(payload.topic, data[0].topic);

            // send the reply to client
            PresentationLayer::sendData(sockinfo.connfd, { payload }, errMsg, isConnectionClosed);
        }
        else if (data[0].msgType == MessageType::PUSH_MESSAGE)
        {
            ClientPayload payload;
            memset(&payload, 0, sizeof(payload));
            payload.time = current_time();

            if (database.addMessage(data[0].topic, data[0].msg, payload.time) == -1)
                payload.msgType = MessageType::TOPIC_NOT_FOUND;
            else
                payload.msgType = MessageType::SUCCESS;
            
            strcpy(payload.topic, data[0].topic);

            // send the reply to client
            PresentationLayer::sendData(sockinfo.connfd, { payload }, errMsg, isConnectionClosed);
        }
        else if (data[0].msgType == MessageType::PUSH_FILE_CONTENTS)
        {
            vector<string> msgs;
            for (auto &x: data)
                msgs.push_back(x.msg);

            ClientPayload payload;
            memset(&payload, 0, sizeof(payload));
            payload.time = current_time();

            if (database.addMessages(data[0].topic, msgs, payload.time) == -1)
                payload.msgType = MessageType::TOPIC_NOT_FOUND;
            else
                payload.msgType = MessageType::SUCCESS;
            
            strcpy(payload.topic, data[0].topic);
            
            // send the reply to client
            PresentationLayer::sendData(sockinfo.connfd, { payload }, errMsg, isConnectionClosed);
        }
        else if (data[0].msgType == MessageType::GET_ALL_TOPICS)
        {
            cerr << "GET ALL TOPICS" << endl;
            ClientPayload payload;
            memset(&payload, 0, sizeof(payload));
            payload.time = current_time();

            auto topics = database.getAllTopics();

            if (topics.size() == 0)
            {
                payload.msgType = MessageType::TOPIC_NOT_FOUND;
                PresentationLayer::sendData(sockinfo.connfd, { payload }, errMsg, isConnectionClosed);
                continue;
            }

            vector<ClientPayload> send_data;
            
            payload.msgType = MessageType::SUCCESS;
            for (auto &topic: topics)
            {
                strcpy(payload.topic, topic.c_str());
                send_data.push_back(payload);
            }

            serv_response[threadid].clear();
            accumulateResponses(threadid, send_data);

            PresentationLayer::sendData(sockinfo.connfd, serv_response[threadid], errMsg, isConnectionClosed);
        }
        else if (data[0].msgType == MessageType::GET_NEXT_MESSAGE)
        {
            ClientPayload payload;
            memset(&payload, 0, sizeof(payload));
            payload.time = data[0].time;

            string msg = database.getNextMessage(data[0].topic, payload.time);

            if (msg == "")
            {
                payload.msgType = database.topicExists(data[0].topic) ? MessageType::MESSAGE_NOT_FOUND : MessageType::TOPIC_NOT_FOUND;
                PresentationLayer::sendData(sockinfo.connfd, { payload }, errMsg, isConnectionClosed);
                continue;
            }

            strcpy(payload.msg, msg.c_str());
            payload.msgType = MessageType::SUCCESS;

            serv_response[threadid].clear();
            accumulateResponses(threadid, { payload });

            PresentationLayer::sendData(sockinfo.connfd, serv_response[threadid], errMsg, isConnectionClosed);
            serv_response[threadid].clear();
        }
        else if (data[0].msgType == MessageType::GET_BULK_MESSAGES)
        {
            ClientPayload payload;
            memset(&payload, 0, sizeof(payload));
            payload.time = data[0].time;

            auto msgs = database.getBulkMessages(data[0].topic, payload.time);

            if (msgs.size() == 0)
            {
                payload.msgType = database.topicExists(data[0].topic) ? MessageType::MESSAGE_NOT_FOUND : MessageType::TOPIC_NOT_FOUND;
                PresentationLayer::sendData(sockinfo.connfd, { payload }, errMsg, isConnectionClosed);
                continue;
            }

            vector<ClientPayload> msgs_to_send;
            payload.msgType = MessageType::SUCCESS;

            for (auto &[time, msg]: msgs)
            {
                strcpy(payload.msg, msg.c_str());
                payload.time = time;
                msgs_to_send.push_back(payload);
            }

            serv_response[threadid].clear();
            accumulateResponses(threadid, msgs_to_send);
            PresentationLayer::sendData(sockinfo.connfd, serv_response[threadid], errMsg, isConnectionClosed);
            serv_response[threadid].clear();
        }
    }
}

// Listening PORTS
void recvHandler(int connfd, int threadId)
{
    while (true)
    {
        auto data = PresentationLayer::getServerReq(connfd);
        cerr << currentDateTime() << ": sizeof array read " << data.size() << endl;

        if (data[0].sender_server_port == info->PORT)
        {
            int threadId = data[0].sender_thread_id;

            // accumulate the results

            serv_response[threadId].clear();
            for (auto &x: data)
                serv_response[threadId].push_back(x.client_payload);

            cerr << "Signaling thread mutex" << endl;
            pthread_cond_signal(&info->thread_pool.getCondFromThreadID(threadId));
            continue;
        }

        // simply forward the data for now
        cerr << "Locking info->send_mutex" << endl;
        pthread_mutex_lock(&info->send_mutex);
        PresentationLayer::sendServerData(info->sendServerPORTInfo.connfd, data);
        cerr << "Unlocking info->send_mutex" << endl;
        pthread_mutex_unlock(&info->send_mutex);
    }
}

SocketInfo activeConnect(const char* ip, int PORT)
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

SocketInfo makePassiveSocket(int PORT)
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
    info->listenSockInfo = makePassiveSocket(info->PORT);

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
    info->sendServerPORTInfo = activeConnect("127.0.0.1", info->server_send_port);
    pthread_join(thread1, nullptr);

    cout << ntohs(info->listenSockInfo.my_addr.sin_port) << " : Connected to port " << ntohs(info->listenSockInfo.dest_addr.sin_port) << endl;
    cout << info->PORT << " : Connected to port " << ntohs(info->sendServerPORTInfo.dest_addr.sin_port) << endl;

    // assign thread to neighbours
    info->thread_pool.startOperation(info->listenSockInfo.connfd, recvHandler);

    // synchronize with creator
    info->q->send_data(getppid(), getpid(), "msgsnd error");
    info->q->receive_data(getpid(), "msgrcv error");

    // Now, we listen for connection from clients
    while (true)
    {
        pthread_t thread;
        socklen_t addrlen = sizeof(info->listenSockInfo.dest_addr);

        while ((info->listenSockInfo.connfd = accept(info->listenSockInfo.sockfd, (struct sockaddr*)&info->listenSockInfo.dest_addr, &addrlen)) < 0)
        {
            if (errno == EINTR)
                continue;
            else
                perror("accept error");
        }

        info->thread_pool.startOperation(info->listenSockInfo.connfd, [](int connfd, int threadId) -> void
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
                clientHandler(sockinfo, threadId);
            }
            else
            {
                strcpy(BUFF, "ERR");
                write(connfd, BUFF, sizeof(BUFF));
                cout << "Unknown client!!" << endl;
            }

            close(connfd);
            cout << ntohs(sockinfo.my_addr.sin_port)  << ": Connection closed from client " << inet_ntoa(sockinfo.dest_addr.sin_addr) << ":" << ntohs(sockinfo.dest_addr.sin_port) << endl;
        });
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
            info = new ServerInfo { &queue, start_port + i, start_port + (i + 1) % count, start_port + (i - 1 + count) % count };
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