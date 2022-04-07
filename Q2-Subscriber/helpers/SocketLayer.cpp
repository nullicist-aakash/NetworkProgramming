#include "SocketLayer.h"
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <cassert>
#include <functional>

using namespace std;

inline int getClientPayloadSize(const ClientPayload& payload)
{
    return sizeof(ClientPayload) - sizeof(ClientPayload::msg) + strlen(payload.msg);
}

template <typename T>
vector<T> getGenericData(const int connfd, string &errMsg, bool &connectionClosed)
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
        int n = read(connfd, (void*)&header, sizeof(MSGHeader));

        if (n < 0)
        {
            errMsg = "read error: " + string(strerror(errno));
            return {};
        }

        if (n == 0)
        {
            errMsg = "read error: Connection closed from remote host";
            connectionClosed = true;
            return {};
        }

        T temp;
        n = read(connfd, (void*)&temp, header.payload_size - sizeof(MSGHeader));
        ((char*)&temp)[n] = '\0';
        payload.push_back(temp);
    }

    return payload;
}

template <typename T>
int sendGenericData(const int connfd, const vector<T> &msgs, function<int(T)> getSize, string &errMsg, bool &connectionClosed)
{
    errMsg = "";
    connectionClosed = false;

    assert(msgs.size() > 0);

    for (int i = 0; i < msgs.size(); ++i)
    {
        MSGHeader h;
        h.isLast = (i == (msgs.size() - 1));
        h.payload_size = sizeof(MSGHeader) + getSize(msgs[i]);

        int n = write(connfd, (void*)&h, sizeof(h));
        
        if (n < 0)
        {
            errMsg = "write error: " + string(strerror(errno));
            return -1;
        }

        if (n == 0)
        {
            errMsg = "read error: Connection closed from remote host";
            connectionClosed = true;
            return -1;
        }

        n = write(connfd, (void*)&msgs[i], h.payload_size - sizeof(MSGHeader));
    
        if (n < 0)
        {
            errMsg = "write error: " + string(strerror(errno));
            return -1;
        }

        if (n == 0)
        {
            errMsg = "read error: Connection closed from remote host";
            connectionClosed = true;
            return -1;
        }
    }

    // success
    return 0;
}

namespace PresentationLayer
{
    vector<ClientPayload> getData(const int connfd, string &errMsg, bool &connectionClosed)
    {
        return getGenericData<ClientPayload>(connfd, errMsg, connectionClosed);
    }

    int sendData(const int connfd, const vector<ClientPayload> &msgs, string &errMsg, bool &connectionClosed)
    {
        return sendGenericData<ClientPayload>(
            connfd, 
            msgs, 
            [](ClientPayload payload) -> int { return getClientPayloadSize(payload); },
            errMsg,
            connectionClosed);
    }

    vector<ServerPayload> getServerReq(const int connfd)
    {
        string x;
        bool y;
        return getGenericData<ServerPayload>(connfd, x, y);
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
            [](ServerPayload payload) -> int { return sizeof(ServerPayload::request_time) + getClientPayloadSize(payload.client_payload); },
            x,
            y);
        
        pthread_mutex_unlock(&send_data_mutex);
    }
}