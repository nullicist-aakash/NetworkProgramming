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

vector<ClientPayload> prepareLocalResponses(const vector<ClientPayload> &data)
{
    Database &database = Database::getInstance();

    ClientPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.time = current_time();
    payload.msgType = MessageType::SUCCESS;
    strcpy(payload.topic, data[0].topic);

    if (data[0].msgType == MessageType::CREATE_TOPIC)
    {
        cerr << "Request to add topic: " << data[0].topic << endl;

        if (data[0].topic[0] == '\0')
        {
            cerr << "Invalid Topic Name!" << endl;
            payload.msgType = MessageType::INVALID_TOPIC_NAME;
            return { payload };
        }

        if (database.addTopic(data[0].topic) == -1)
        {
            cerr << "Topic already exists!" << endl;
            payload.msgType = MessageType::TOPIC_ALREADY_EXISTS;
        }

        cerr << "Topic added successfully!" << endl;

        strcpy(payload.topic, data[0].topic);
        return { payload };
    }
    else if (data[0].msgType == MessageType::PUSH_MESSAGE)
    {
        cerr << "Request to push message '" << data[0].msg << "' related to topic: '" << data[0].topic << "'" << endl;
        if (database.addMessage(data[0].topic, data[0].msg, payload.time) == -1)
        {
            cerr << "Topic doesn't exist" << endl;
            payload.msgType = MessageType::TOPIC_NOT_FOUND;
        }
        
        cerr << "Message added successfully!" << endl;
        strcpy(payload.topic, data[0].topic);
        return { payload };
    }
    else if (data[0].msgType == MessageType::PUSH_FILE_CONTENTS)
    {
        cerr << "Request to push file contents related to topic: '" << data[0].topic << "'" << endl;
        vector<string> msgs;
        for (auto &x: data)
            msgs.push_back(x.msg);

        if (database.addMessages(data[0].topic, msgs, payload.time) == -1)
        {
            cerr << "Topic doesn't exist" << endl;
            payload.msgType = MessageType::TOPIC_NOT_FOUND;
        }
        
        cerr << "Messages added successfully!" << endl;
        strcpy(payload.topic, data[0].topic);

        return { payload };
    }
    else if (data[0].msgType == MessageType::GET_ALL_TOPICS)
    {
        cerr << "Request to get all topics" << endl;
        auto topics = database.getAllTopics();

        if (topics.size() == 0)
        {
            cerr << "Topics don't exist" << endl;
            payload.msgType = MessageType::TOPIC_NOT_FOUND;
            return { payload };
        }

        cerr << "Sending " << topics.size() << " topics" << endl;

        vector<ClientPayload> send_data;
        
        for (auto &topic: topics)
        {
            strcpy(payload.topic, topic.c_str());
            send_data.push_back(payload);
        }

        return send_data;
    }
    else if (data[0].msgType == MessageType::GET_NEXT_MESSAGE)
    {
        cerr << "Request to get next message related to topic '" << data[0].topic << "' for time: " << DateTime(data[0].time) << endl;

        payload.time = data[0].time;
        string msg = database.getNextMessage(data[0].topic, payload.time);

        if (msg == "")
        {
            cerr << "No message found" << endl;
            payload.msgType = database.topicExists(data[0].topic) ? MessageType::MESSAGE_NOT_FOUND : MessageType::TOPIC_NOT_FOUND;
            return { payload };
        }

        cerr << "Sending message" << endl;
        strcpy(payload.msg, msg.c_str());
        return { payload };
    }
    else if (data[0].msgType == MessageType::GET_BULK_MESSAGES)
    {
        cerr << "Request to get bulk messages related to topic '" << data[0].topic << "' for time: " << DateTime(data[0].time) << endl;

        payload.time = data[0].time;
        auto msgs = database.getBulkMessages(data[0].topic, payload.time);

        if (msgs.size() == 0)
        {
            cerr << "No message found" << endl;
            payload.msgType = database.topicExists(data[0].topic) ? MessageType::MESSAGE_NOT_FOUND : MessageType::TOPIC_NOT_FOUND;
            return { payload };
        }

        vector<ClientPayload> send_data;

        for (auto &[time, msg]: msgs)
        {
            strcpy(payload.msg, msg.c_str());
            payload.time = time;
            send_data.push_back(payload);
        }

        cerr << "Sending messages" << endl;
        return send_data;
    }

    assert(false);
}

// On access validation from a client
void clientHandler(const SocketInfo& sockinfo, int threadid)
{
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

        if (data[0].msgType == MessageType::CREATE_TOPIC || data[0].msgType == MessageType::PUSH_MESSAGE || data[0].msgType == MessageType::PUSH_FILE_CONTENTS)
        {
            PresentationLayer::sendData(sockinfo.connfd, prepareLocalResponses(data), errMsg, isConnectionClosed);
        }
        else if (data[0].msgType == MessageType::GET_ALL_TOPICS)
        {
            vector<ClientPayload> payload = prepareLocalResponses(data);
            payload.push_back(data[0]);

            // accummulate result from server
            accumulateResponses(threadid, payload);

            // remove the last entry
            serv_response[threadid].pop_back();

            PresentationLayer::sendData(sockinfo.connfd, serv_response[threadid], errMsg, isConnectionClosed);
        }
        else if (data[0].msgType == MessageType::GET_NEXT_MESSAGE || data[0].msgType == MessageType::GET_BULK_MESSAGES)
        {
            // Add topic from other ends to local database to sync
            {
                ClientPayload getAllTopics;
                getAllTopics.msgType = MessageType::GET_ALL_TOPICS;
                getAllTopics.time = current_time();
                getAllTopics.topic[0] = '\0';
                accumulateResponses(threadid, {getAllTopics});

                for (auto &x: serv_response[threadid])
                    Database::getInstance().addTopic(x.topic);
            }

            vector<ClientPayload> payload = prepareLocalResponses(data);
            payload.push_back(data[0]);

            // accummulate result from server
            accumulateResponses(threadid, payload);

            // remove the last entry
            serv_response[threadid].pop_back();

            PresentationLayer::sendData(sockinfo.connfd, serv_response[threadid], errMsg, isConnectionClosed);
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

        // get local response
        auto local = prepareLocalResponses({ data.back().client_payload });

        // simply forward if no valid reply in current server
        if (local[0].msgType != MessageType::SUCCESS)
        {
            // simply forward the data
            cerr << "Locking info->send_mutex" << endl;
            pthread_mutex_lock(&info->send_mutex);
            PresentationLayer::sendServerData(info->sendServerPORTInfo.connfd, data);
            cerr << "Unlocking info->send_mutex" << endl;
            pthread_mutex_unlock(&info->send_mutex);
            continue;
        }

        auto cli_req = data.back().client_payload;
        data.pop_back();

        vector<ServerPayload> serv_payload;
        ServerPayload p;
        p.sender_server_port = data[0].sender_server_port;
        p.sender_thread_id = data[0].sender_thread_id;

        // if received data is no success, return local data
        if (data[0].client_payload.msgType != MessageType::SUCCESS)
        {
            for (auto &x: local)
            {
                memcpy(&p.client_payload, &x, sizeof(ClientPayload));
                serv_payload.push_back(p);
            }

            memcpy(&p.client_payload, &cli_req, sizeof(ClientPayload));
            serv_payload.push_back(p);

            cerr << "Locking info->send_mutex" << endl;
            pthread_mutex_lock(&info->send_mutex);
            PresentationLayer::sendServerData(info->sendServerPORTInfo.connfd, serv_payload);
            cerr << "Unlocking info->send_mutex" << endl;
            pthread_mutex_unlock(&info->send_mutex);
            continue;
        }

        memset(&p.client_payload, 0, sizeof(p.client_payload));
        p.client_payload.time = current_time();
        p.client_payload.msgType = MessageType::SUCCESS;
        strcpy(p.client_payload.topic, data[0].client_payload.topic);  
        
        // Here means we need to merge the data
        if (cli_req.msgType == MessageType::GET_ALL_TOPICS)
        {
            // Take union
            set<string> topics;
            for (auto &x: local)
                topics.insert(x.topic);

            for (auto &x: data)
                topics.insert(x.client_payload.topic);

            cerr << "on receive- Topic count to send: " << topics.size() << endl;
            ServerPayload send_data;
        
            for (auto &topic: topics)
            {
                strcpy(p.client_payload.topic, topic.c_str());
                serv_payload.push_back(p);
            }
        }
        else if (cli_req.msgType == MessageType::GET_NEXT_MESSAGE)
        {
            // Take minnimum of two
            auto _mintime = local[0].time < data[0].client_payload.time ? local[0].time : data[0].client_payload.time;

            if (local[0].time == _mintime)
                memcpy(&p.client_payload, &local[0], sizeof(ClientPayload));
            else
                memcpy(&p.client_payload, &data[0].client_payload, sizeof(ClientPayload));

            serv_payload.push_back(p);
        }
        else if (cli_req.msgType == MessageType::GET_BULK_MESSAGES)
        {
            using pcs = pair<std::chrono::_V2::system_clock::time_point, string>;
            priority_queue<pcs, vector<pcs>, greater<pcs>> pq;

            for (auto &x: local)
                pq.push({ x.time, x.msg });
            
            for (auto &x: data)
                pq.push({ x.client_payload.time, x.client_payload.msg });

            // get top BULK_MSG_SIZE msgs
            for (int i = 0; !pq.empty() && i < BULK_LIMIT; ++i, pq.pop())
            {
                auto &time = pq.top().first;
                auto &msg = pq.top().second;

                strcpy(p.client_payload.msg, msg.c_str());
                p.client_payload.time = time;
                serv_payload.push_back(p);
            }
        }
        else
            assert(0);

        memcpy(&p.client_payload, &cli_req, sizeof(ClientPayload));
        serv_payload.push_back(p);
        cerr << "Locking info->send_mutex" << endl;
        pthread_mutex_lock(&info->send_mutex);
        PresentationLayer::sendServerData(info->sendServerPORTInfo.connfd, serv_payload);
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
    
    freopen(("logs/" + to_string(info->PORT) + ".log").c_str(), "w", stderr);
    
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