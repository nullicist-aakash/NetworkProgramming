#pragma once
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctime>

using namespace std;
 
const int maxMessageSize = 512;	
const int maxTopicSize = 20;
#define MAX_CONNECTION_COUNT 32

const std::string currentDateTime();

struct SocketInfo
{
    int sockfd;
    int connfd;
    struct sockaddr_in my_addr;
    struct sockaddr_in dest_addr;
};

struct ClientMessageHeader
{
    bool isLastData;
    clock_t time;
    int cur_size;
    char req[4];
    char topic[maxTopicSize + 1];

    
	static bool isValidTopic(const string& topic)
	{
		return topic.size() <= maxTopicSize;
	}
};

struct ClientMessage : ClientMessageHeader
{
    char msg[maxMessageSize + 1];

    void print()
    {
        std::time_t t = std::time(0);
        std::tm* now = std::localtime(&t);
        cerr << currentDateTime() << " - { isLastData: " << isLastData << ", cur_size: " << cur_size << ", req: " << req << ", topic: " << topic << ", msg: " << msg << " }" << endl;
    }
};

const std::string currentDateTime()
{
    time_t     now = std::time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
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
                out.push_back(msg);
                msg.print();
                continue;
            }

            n = read(connfd, (void*)&msg.msg, msg.cur_size - sizeof(ClientMessageHeader));

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

            out.push_back(msg);
            msg.print();
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
    int client_writeData(int connfd, const char* req, const char* topic, const vector<string>& msgs, string& errMsg, bool &closeConnection)
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
        cli_msg.time = clock();

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
            msg.print();
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