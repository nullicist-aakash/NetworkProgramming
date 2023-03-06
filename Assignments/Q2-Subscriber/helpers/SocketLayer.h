#pragma once
#include <chrono>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */

using short_time = std::chrono::_V2::system_clock::time_point;

#define MAX_CONNECTION_COUNT 32
const int maxMessageSize = 512;	
const int maxTopicSize = 26;


struct SocketInfo
{
    int sockfd;
    int connfd;
    struct sockaddr_in my_addr;
    struct sockaddr_in dest_addr;
};

enum class MessageType
{
    CREATE_TOPIC,
    PUSH_MESSAGE,
    PUSH_FILE_CONTENTS,
    GET_ALL_TOPICS,
    GET_NEXT_MESSAGE,
    GET_BULK_MESSAGES,

    SUCCESS,
    INVALID_TOPIC_NAME,
    TOPIC_NOT_FOUND,
    TOPIC_ALREADY_EXISTS,
    MESSAGE_NOT_FOUND,
    UNKNOWN_ERROR
};

struct MSGHeader
{
    bool isLast;
    int payload_size;
};

struct ClientPayload
{
    short_time time;
    MessageType msgType;
    char topic[maxTopicSize + 1];
    char msg[maxMessageSize + 1];
};

struct ServerPayload
{
    int sender_server_port;
    int sender_thread_id;
    ClientPayload client_payload;
};

namespace PresentationLayer
{
    std::vector<ClientPayload> getData(const int, std::string &, bool &);

    int sendData(const int, const std::vector<ClientPayload>&, std::string &, bool &);

    std::vector<ServerPayload> getServerReq(const int);

    void sendServerData(const int, const std::vector<ServerPayload>&);
};