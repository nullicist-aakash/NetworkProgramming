#pragma once
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctime>

using namespace std;
 
const int maxMessageSize = 512;	
const int maxTopicSize = 20;
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

const std::string currentDateTime()
{
    time_t     now = std::time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

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


namespace SocketIO
{
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