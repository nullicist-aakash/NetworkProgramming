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
#include "helpers/SocketIO.h"
#include "helpers/Database.h"
#include "helpers/ThreadPool.h"
#include "helpers/Queue.h"

using namespace std;

struct ServerInfo
{
    const Queue<int> *q;
    ThreadPool thread_pool;

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

void printServerMessage(ServerMessage* servmsg)
{
    cerr << currentDateTime() << " - \t{ Source Server PORT: " << servmsg->sender_server_port << 
            ", Source Server thread: " << servmsg->sender_thread_id << " }" << endl;

    print(servmsg->cli_msg, true);
}

vector<ClientMessage>* receivedData;
pthread_cond_t received_data_cond = PTHREAD_COND_INITIALIZER;

void sendDataToServer(int sender_server_port, int sender_thread_id, const short_time &time, const vector<ClientMessage>& msgs)
{
    static pthread_mutex_t send_data_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&send_data_mutex);
    ServerMessage serv_msg;
    serv_msg.sender_server_port = sender_server_port;
    serv_msg.sender_thread_id = sender_thread_id;

    for (auto &cli_msg: msgs)
    {
        int sz = sizeof(ServerMessage) - sizeof(ClientMessage) + cli_msg.cur_size;
        memcpy(&serv_msg.cli_msg, &cli_msg, sz);
        cerr << currentDateTime() << " - " << sz << " bytes wriiten to another server with PORT " << info->server_send_port << ". Content - " << endl;
        printServerMessage(&serv_msg);

        write(info->sendServerPORTInfo.connfd, (void*)&serv_msg, sz);
    }

    pthread_mutex_unlock(&send_data_mutex);
}

void sendDataToServer(int sender_server_port, int sender_thread_id, const short_time &time, const ClientMessageHeader& header, vector<string>& msgs)
{
    vector<ClientMessage> packedmsgs;

    ClientMessage packedmsg;
    memcpy(&packedmsg, &header, sizeof(header));
    if (msgs.size() == 0)
        msgs.push_back("");

    for (int i = 0; i < msgs.size(); ++i)
    {
        auto &msg = msgs[i];
        packedmsg.isLastData = (i == msgs.size() - 1);

        int sz = msg.size() + sizeof(header);
        strcpy(packedmsg.msg, msg.c_str());
        packedmsg.cur_size = sz;

        packedmsgs.push_back(packedmsg);
    }

    sendDataToServer(sender_server_port, sender_thread_id, time, packedmsgs);
}

void* serveReceiveRequest(void* data)
{
    static pthread_mutex_t received_data_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_detach(pthread_self());

    void** arr = (void**)data;
    assert(arr[2] != NULL);

    int sender_server_port = *(int*)arr[0];
    int sender_thread_id = *(int*)arr[1];
    short_time time = *(short_time*)arr[2];
    auto msgs = (vector<ClientMessage>*)arr[3];

    delete (int*)arr[0];
    delete (int*)arr[1];
    delete (short_time*)arr[2];
    delete[] arr;

    // if self port, signal child
    if (sender_server_port == info->PORT)
    {
        receivedData = msgs;
        pthread_mutex_lock(&received_data_mutex);
        pthread_cond_signal(&info->thread_pool.getCondFromThreadID(sender_thread_id));
        pthread_cond_wait(&received_data_cond, &received_data_mutex);
        pthread_mutex_unlock(&received_data_mutex);
        return NULL;
    }

    // if Get all topics
    if (strcmp(msgs->at(0).req, "GAT") == 0)
    {
        set<string> topics;
        for (auto &x: *msgs)
            topics.insert(x.msg);
        
        for (auto &x: Database::getInstance().getAllTopics())
            topics.insert(x);
        
        vector<string> topics_to_send;
        for (auto &x: topics)
            if (x.size() != 0)
                topics_to_send.push_back(x);

        sendDataToServer(sender_server_port, sender_thread_id, time, (ClientMessageHeader)msgs->at(0), topics_to_send);
    }
    else
    {
        sendDataToServer(sender_server_port, sender_thread_id, time, *msgs);
    }
    
    delete msgs;
    return NULL;
}

namespace RequestHandler
{
    vector<pair<string, short_time>> HandleSendingDataToNeighbour(int tid, vector<string> &in_msgs, const char* req, const char* topic, const short_time &clk = std::chrono::high_resolution_clock::now())
    {
        int sender_server_port = info->PORT;
        int sender_thread_id = tid;

        vector<ClientMessage> msgs_to_send;
        ClientMessage msg;
        strcpy(msg.req, req);
        strcpy(msg.topic, topic);
        
        for (int i = 0; i < in_msgs.size(); ++i)
        {
            msg.isLastData = i == (in_msgs.size() - 1);
            msg.cur_size = sizeof(ClientMessageHeader) + in_msgs[i].size();
            strcpy(msg.msg, in_msgs[i].c_str());
            msgs_to_send.push_back(msg);
        }

        if (in_msgs.size() == 0)
        {
            msg.isLastData = true;
            msg.cur_size = sizeof(ClientMessageHeader);
            strcpy(msg.msg, "");
            msgs_to_send.push_back(msg);
        }

        // Send the msgs
        sendDataToServer(sender_server_port, sender_thread_id, clk, msgs_to_send);

        auto *mutex = &info->thread_pool.getMutexFromThreadID(tid);
        auto *cond = &info->thread_pool.getCondFromThreadID(tid);

        pthread_cond_wait(cond, mutex);

        vector<pair<string, short_time>> ret;
        for (auto &x: *receivedData)
            ret.push_back({ x.msg, x.time });

        pthread_cond_signal(&received_data_cond);
        return ret;
    }

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

    void getAllTopics(int connfd, int threadid, string& errMsg, bool &isConnectionClosed)
    {
        vector<string> topics = Database::getInstance().getAllTopics();
        auto ret  = HandleSendingDataToNeighbour(threadid, topics, "GAT", "");

        for (auto &[topic, time]: ret)
            topics.push_back(topic);

        string res = topics.size() == 0 ? "NTO" : "OK";
        SocketIO::client_writeData(connfd, res.c_str(), "", topics, errMsg, isConnectionClosed);
    }

    void getNextMessage(const vector<ClientMessage> &data, int connfd, int threadid, string& errMsg, bool &isConnectionClosed)
    {
        // check for existence of topic        
        vector<string> topics = Database::getInstance().getAllTopics();
        auto ret  = HandleSendingDataToNeighbour(threadid, topics, "GAT", "");

        for (auto &[topic, time]: ret)
            topics.push_back(topic);

        bool topicExists = false;        
        for (auto &topic: topics)
            if (strcmp(topic.c_str(), data[0].topic) == 0)
            {
                topicExists = true;
                break;
            }

        if (!topicExists && !Database::getInstance().topicExists(data[0].topic))
        {
            string res = "NTO";
            SocketIO::client_writeData(connfd, res.c_str(), data[0].topic, {}, errMsg, isConnectionClosed);
            return;
        }

        // topic exists on atleast one server. Send the message back
        auto time = data[0].time;
        vector<string> msgs;
        ret = HandleSendingDataToNeighbour(threadid, msgs, "GNM", data[0].topic, time);

        assert(ret.size() < 2);
        string my_msg = Database::getInstance().getNextMessage(data[0].topic, time);
        
        if (ret.size() == 0)
            ret.push_back({ my_msg, time });
        else if (my_msg != "")
            if (ret[0].second > time)
            {
                ret[0].first = my_msg;
                ret[0].second = time;
            }

        string res = ret[0].first == "" ? "NMG" : "OK";
        SocketIO::client_writeData(connfd, res.c_str(), data[0].topic, {ret[0].first}, errMsg, isConnectionClosed, ret[0].second);
    }

    void getAllMessages(const vector<ClientMessage> &data, int connfd, int threadid, string& errMsg, bool &isConnectionClosed)
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

// On access validation from a client
void serveClient(const SocketInfo& sockinfo, int threadid)
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
            RequestHandler::getAllTopics(sockinfo.connfd, threadid, errMsg, isConnectionClosed);
        else if (strcmp(data[0].req, "GNM") == 0)
            RequestHandler::getNextMessage(data, sockinfo.connfd, threadid, errMsg, isConnectionClosed);
        else if (strcmp(data[0].req, "GAM") == 0)
            RequestHandler::getAllMessages(data, sockinfo.connfd, threadid, errMsg, isConnectionClosed);
        else
            SocketIO::client_writeData(sockinfo.connfd, "ERR", "", {}, errMsg, isConnectionClosed);

        if (errMsg == "")
            continue;

        if (isConnectionClosed)
            break;

        cout << errMsg << endl;
    }
}

// Listening PORTS

void recvHandler(int connfd, int threadId)
{
    // Since this port is serialised, no out of order or mixed packets will come
    vector<ClientMessage> out;
    ServerMessage msg;

    while (1)
    {
        out.clear();
        msg.cli_msg.isLastData = false;

        while (!msg.cli_msg.isLastData)
        {
            bzero((void*)&msg, sizeof(msg));
            cerr << "Waiting to read from " << info->server_recv_port << endl;
            int n = read(connfd, (void*)&msg, sizeof(msg) - sizeof(ClientMessage) + sizeof(ClientMessageHeader));
            cerr << "\t(Received " << n << " bytes from " << info->server_recv_port << ")" << endl;
            

            if (msg.cli_msg.cur_size == sizeof(ClientMessageHeader))
            {
                cerr << currentDateTime() << " - " << n << " bytes read from another server with PORT " << info->server_recv_port << ". Content - " << endl;
                printServerMessage(&msg);

                out.push_back(msg.cli_msg);
                continue;
            }

            int n2 = read(connfd, (void*)&(msg.cli_msg.msg), msg.cli_msg.cur_size - sizeof(ClientMessageHeader));
            cerr << currentDateTime() << " - " << n + n2 << " bytes from another server with PORT " << info->server_send_port << ". Content - " << endl;
            printServerMessage(&msg);
            
            msg.cli_msg.msg[n] = '\0';
            out.push_back(msg.cli_msg);
        }

        int sender_server_port = msg.sender_server_port;
        int sender_thread_id = msg.sender_thread_id;
        short_time time = msg.time_of_req;

        // create a thread to start processing received data
        pthread_t thread;
        void** data = new void*[4];
        data[0] = new int { sender_server_port };
        data[1] = new int { sender_thread_id };
        data[2] = new short_time { time };
        data[3] = new vector<ClientMessage> { out };

        pthread_create(&thread, NULL, &serveReceiveRequest, (void*)data);
    }
}

void clientConnectionHandler(int connfd, int threadId)
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
        serveClient(sockinfo, threadId);
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
    info->sendServerPORTInfo = SocketIO::activeConnect("127.0.0.1", info->server_send_port);
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

        info->thread_pool.startOperation(info->listenSockInfo.connfd, clientConnectionHandler);
    }
}

int main(int argc, char** argv)
{
    printSizeInfo();
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