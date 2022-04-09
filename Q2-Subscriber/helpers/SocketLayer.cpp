#include "SocketLayer.h"
#include "Time.h"
#include <strings.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cassert>
#include <functional>
#include <iostream>

using namespace std;

inline int getClientPayloadSize(const ClientPayload& payload)
{
    return sizeof(ClientPayload) - sizeof(ClientPayload::msg) + strlen(payload.msg);
}

template <typename T>
vector<T> getGenericData(const int connfd, string &errMsg, bool &connectionClosed, function<void(T&)> printType, bool isServer)
{
    errMsg = "";
    connectionClosed = false;

    vector<T> payload;
    MSGHeader header;
    header.isLast = false;

    while (!header.isLast)
    {
        // set all fields to zero
        bzero(&header, sizeof(header));
        int n1 = read(connfd, (void*)&header, sizeof(MSGHeader));

        if (n1 < 0)
        {
            errMsg = "read error: " + string(strerror(errno));
            return {};
        }

        if (n1 == 0)
        {
            errMsg = "Connection closed from remote host";
            connectionClosed = true;
            return {};
        }

        T temp;
        bzero(&temp, sizeof(T));
        int n2 = read(connfd, (void*)&temp, header.payload_size - sizeof(MSGHeader));
        ((char*)&temp)[n2] = '\0';
        payload.push_back(temp);

        if (!isServer)
            cerr << currentDateTime() << ": " << connfd << " - Read " << n1 + n2 << " bytes from other end" << endl;
        else
            cerr << currentDateTime() << ": " << connfd << " - (Read " << n1  << " + " <<  n2 << " bytes from neighbor)" << endl;

        printType(temp);
    }
    
    cerr << endl;
    return payload;
}

template <typename T>
int sendGenericData(const int connfd, const vector<T> &msgs, function<int(T)> getSize, string &errMsg, bool &connectionClosed, function<void(const T&)> printType, bool isServer)
{
    errMsg = "";
    connectionClosed = false;

    assert(msgs.size() > 0);

    for (int i = 0; i < msgs.size(); ++i)
    {
        MSGHeader h;
        h.isLast = (i == (msgs.size() - 1));
        h.payload_size = sizeof(MSGHeader) + getSize(msgs[i]);

        int n1 = write(connfd, (void*)&h, sizeof(h));
        
        if (n1 < 0)
        {
            errMsg = "write error: " + string(strerror(errno));
            return -1;
        }

        if (n1 == 0)
        {
            errMsg = "read error: Connection closed from remote host";
            connectionClosed = true;
            return -1;
        }

        int n2 = write(connfd, (void*)&msgs[i], h.payload_size - sizeof(MSGHeader));
    
        if (n2 < 0)
        {
            errMsg = "write error: " + string(strerror(errno));
            return -1;
        }

        if (n2 == 0)
        {
            errMsg = "read error: Connection closed from remote host";
            connectionClosed = true;
            return -1;
        }


        if (!isServer)
            cerr << currentDateTime() << ": " << connfd << " - Written " << n1 + n2 << " bytes to other end" << endl;
        else
            cerr << currentDateTime() << ": " << connfd << " - (Written " << n1  << " + " <<  n2 << " bytes to neighbor)" << endl;

        printType(msgs[i]);
    }
    
    cerr << endl;
    return 0;
}

namespace PresentationLayer
{
    vector<ClientPayload> getData(const int connfd, string &errMsg, bool &connectionClosed)
    {
        return getGenericData<ClientPayload>(connfd, errMsg, connectionClosed, 
            [](ClientPayload &payload) -> void
            {
                char buff[sizeof(ClientPayload) + 50];

                sprintf(buff, 
                    "\t{ time: %s, msgType: %d, topic: '%s', msg: '%s' }",
                    DateTime(payload.time).c_str(),
                    payload.msgType,
                    payload.topic,
                    payload.msg
                    );
                cerr << buff << endl;
            },
            false);
    }

    int sendData(const int connfd, const vector<ClientPayload> &msgs, string &errMsg, bool &connectionClosed)
    {
        return sendGenericData<ClientPayload>(
            connfd, 
            msgs, 
            [](ClientPayload payload) -> int { return getClientPayloadSize(payload); },
            errMsg,
            connectionClosed, 
            [](const ClientPayload &payload) -> void
            {
                char buff[sizeof(ClientPayload) + 50];

                sprintf(buff, 
                    "\t{ time: %s, msgType: %d, topic: '%s', msg: '%s' }",
                    DateTime(payload.time).c_str(),
                    payload.msgType,
                    payload.topic,
                    payload.msg
                    );
                cerr << buff << endl;
            },
            false);
    }

    vector<ServerPayload> getServerReq(const int connfd)
    {
        string x;
        bool y;
        auto a = getGenericData<ServerPayload>(connfd, x, y,
            [](ServerPayload &payload) -> void
            {
                char buff[sizeof(ServerPayload) + 50];

                sprintf(buff, 
                    "\t{ { sender_port: %d, sender_thread_id: %d }, time: %s, msgType: %d, topic: '%s', msg: '%s' }",
                    payload.sender_server_port,
                    payload.sender_thread_id,
                    DateTime(payload.client_payload.time).c_str(),
                    payload.client_payload.msgType,
                    payload.client_payload.topic,
                    payload.client_payload.msg
                    );

                cerr << buff << endl;
            },
            true);

        if (x != "")
            cout << "error occured " << x << endl;
            
        return a;
    }

    void sendServerData(const int connfd, const vector<ServerPayload>&msgs)
    {
        string x;
        bool y;

        static pthread_mutex_t send_data_mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&send_data_mutex);

        sendGenericData<ServerPayload>(
            connfd, 
            msgs, 
            [](ServerPayload payload) -> int { return sizeof(ServerPayload) - sizeof(ClientPayload) + getClientPayloadSize(payload.client_payload); },
            x,
            y,
            [](const ServerPayload &payload) -> void
            {
                char buff[sizeof(ServerPayload) + 50];

                sprintf(buff, 
                    "\t{ { sender_port: %d, sender_thread_id: %d }, time: %s, msgType: %d, topic: '%s', msg: '%s' }",
                    payload.sender_server_port,
                    payload.sender_thread_id,
                    DateTime(payload.client_payload.time).c_str(),
                    payload.client_payload.msgType,
                    payload.client_payload.topic,
                    payload.client_payload.msg
                    );

                cerr << buff << endl;
            },
            true);
        
        pthread_mutex_unlock(&send_data_mutex);
    }
}