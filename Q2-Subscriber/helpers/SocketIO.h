#pragma once
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <chrono>
#include <pthread.h>
#define MAX_CONNECTION_COUNT 32

using namespace std;
 
const int maxMessageSize = 512;	
const int maxTopicSize = 20;

using short_time = std::chrono::_V2::system_clock::time_point;


const std::string DateTime(short_time time)
{
    time_t     now = std::chrono::system_clock::to_time_t(time);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

struct SocketInfo
{
    int sockfd;
    int connfd;
    struct sockaddr_in my_addr;
    struct sockaddr_in dest_addr;
};

struct ClientMessageHeader
{
    short_time time;
    bool isLastData;
    char req[4];
    char topic[maxTopicSize + 1];
    int cur_size;
};

struct ClientMessage : ClientMessageHeader
{
    char msg[maxMessageSize + 1];
};

struct ServerMessage
{
    int sender_server_port;
    int sender_thread_id;
    short_time time_of_req;
    ClientMessage cli_msg;
};

void print(ClientMessage& c, bool isTab = false)
{
    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);
    cerr << currentDateTime() << " - " << (isTab ? "\t" : "") << "{ isLastData: " << c.isLastData << ", cur_size: " << c.cur_size << ", time: " << DateTime(c.time) << ", req: " << c.req << ", topic: " << c.topic << ", msg: " << c.msg << " }" << endl;
}

void printSizeInfo()
{
    ClientMessageHeader hdr;
    ClientMessage cli_msg;
    ServerMessage ser_msg;

    cout << "Client Message Header Info" << endl;
    cout << "Size: " << sizeof(ClientMessageHeader) << endl;
    cout << "time                :     Size - " << sizeof(ClientMessageHeader::time) << ", Offset: " << ((char*)&hdr.time - (char*)&hdr) << endl; 
    cout << "isLastData          :     Size - " << sizeof(ClientMessageHeader::isLastData) << ", Offset: " << ((char*)&hdr.isLastData - (char*)&hdr) << endl; 
    cout << "req                 :     Size - " << sizeof(ClientMessageHeader::req) << ", Offset: " << ((char*)&hdr.req - (char*)&hdr) << endl; 
    cout << "topic               :     Size - " << sizeof(ClientMessageHeader::topic) << ", Offset: " << ((char*)&hdr.topic - (char*)&hdr) << endl; 
    cout << "cur_size            :     Size - " << sizeof(ClientMessageHeader::time) << ", Offset: " << ((char*)&hdr.cur_size - (char*)&hdr) << endl << endl;

    
    cout << "Client Message Info" << endl;
    cout << "Size: " << sizeof(ClientMessage) << endl;
    cout << "msg                 :     Size - " << sizeof(ClientMessage::msg) << ", Offset: " << ((char*)&cli_msg.msg - (char*)&cli_msg) << endl << endl;

    cout << "Server Message Info" << endl;
    cout << "Size: " << sizeof(ServerMessage) << endl;
    cout << "sender_server_port  :     Size - " << sizeof(ServerMessage::sender_server_port) << ", Offset: " << ((char*)&ser_msg.sender_server_port - (char*)&ser_msg) << endl; 
    cout << "sender_thread_id    :     Size - " << sizeof(ServerMessage::sender_thread_id) << ", Offset: " << ((char*)&ser_msg.sender_thread_id - (char*)&ser_msg) << endl; 
    cout << "cli_msg             :     Size - " << sizeof(ServerMessage::cli_msg) << ", Offset: " << ((char*)&ser_msg.cli_msg - (char*)&ser_msg) << endl << endl;

}

namespace SocketIO
{
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

    

    /* Preconditions:
     * 1.                   = connfd is a valid file descriptor
     *
     * Postconditions:
     * 1.   closeConnection = true means connection is closed prematurely from client side
     * 2.   errMsg          = in case of error, this string contains the message to display on server side
     * 3.   return          = array of data sent by client
    */
    vector<ClientMessage> client_readData(int connfd, string& errMsg, bool &closeConnection)
    {
        errMsg = "";
        closeConnection = false;

        vector<ClientMessage> out;

        ClientMessage msg;
        msg.isLastData = false;
        int n;

        while (!msg.isLastData)
        {
            bzero((void*)&msg, sizeof(msg));
            n = read(connfd, (void*)&msg, sizeof(ClientMessageHeader));

            if (n < 0)
            {
                errMsg = "read error: " + string(strerror(errno));
                return {};
            }

            if (n == 0)
            {
                errMsg = "read error: Connection closed prematurely";
                closeConnection = true;
                return {};
            }

            if (msg.cur_size == sizeof(ClientMessageHeader))
            {
                cerr << n << " bytes of header read. No message." << endl;
                out.push_back(msg);
                print(msg);
                continue;
            }

            n = read(connfd, (void*)&(msg.msg), msg.cur_size - sizeof(ClientMessageHeader));

            if (n < 0)
            {
                errMsg = "read error: " + string(strerror(errno));
                return {};
            }

            if (n == 0)
            {
                errMsg = "read error: Connection closed prematurely";
                closeConnection = true;
                return {};
            }

            cerr << n + sizeof(ClientMessageHeader) << " bytes readed. Message: ";
            msg.msg[n] = '\0';
            cerr << "'" << msg.msg << "'" << endl;
            out.push_back(msg);
            print(msg);
        }

        return out;
    }

    /* Preconditions:
     * 1.                   = connfd is a valid file descriptor
     * 2.                   = req contains the request to send
     * 3.                   = topic is the string which should be filled in topic field of message
     * Postconditions:
     * 1.   closeConnection = true means connection is closed prematurely from client side
     * 2.   errMsg          = in case of error, this string contains the message to display on server side
     * 3.   return          = -1 in case of error, else 0
    */
    int client_writeData(int connfd, const char* req, const char* topic, const vector<string>& msgs, string& errMsg, bool &closeConnection, short_time clk = std::chrono::high_resolution_clock::now())
    {
        if(strlen(topic) > maxTopicSize)
        {
            errMsg = "Topic '" + string(topic) + "' length should be less than " + to_string(maxTopicSize + 1) + " characters";
			return -1;
        }

        errMsg = "";
        closeConnection = false;

        vector<ClientMessage> msgs_to_send;

        ClientMessage cli_msg;
        strcpy(cli_msg.req, req);
        strcpy(cli_msg.topic, topic);
        cli_msg.time = clk;

        for (int i = 0; i < msgs.size(); ++i)
        {
            cli_msg.isLastData = i == (msgs.size() - 1);
            cli_msg.cur_size = sizeof(ClientMessageHeader) + msgs[i].size();
            strcpy(cli_msg.msg, msgs[i].c_str());
            msgs_to_send.push_back(cli_msg);
        }

        if (msgs.size() == 0)
        {
            cli_msg.isLastData = true;
            cli_msg.cur_size = sizeof(ClientMessageHeader);
            strcpy(cli_msg.msg, "");
            msgs_to_send.push_back(cli_msg);
        }

        for (auto &msg: msgs_to_send)
        {
            print(msg);
            int n = write(connfd, (void*)&msg, msg.cur_size);
            
            if (n < 0)
            {
                errMsg = "write error: " + string(strerror(errno));
                return -1;
            }

            if (n == 0)
            {
                errMsg = "write error: Connection closed prematurely";
                closeConnection = true;
                return -1;
            }
        }

        return 0;
    }
};