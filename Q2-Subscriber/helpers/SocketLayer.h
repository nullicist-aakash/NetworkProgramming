#pragma once
#include <chrono>
#include <vector>
#include <string>

using short_time = std::chrono::_V2::system_clock::time_point;

const int maxMessageSize = 512;	
const int maxTopicSize = 21;

enum class MessageType
{
    CREATE_TOPIC,
    PUSH_MESSAGE,
    PUSH_FILE_CONTENTS,
    GET_ALL_TOPICS,
    GET_NEXT_MESSAGE,
    GET_ALL_MESSAGES,

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
    short_time request_time;
    ClientPayload client_payload;
};

namespace PresentationLayer
{
    std::vector<ClientPayload> getData(const int, std::string &, bool &);

    int sendData(const int, const std::vector<ClientPayload>&, std::string &, bool &);

    std::vector<ServerPayload> getServerReq(const int);

    void sendServerData(const int, const std::vector<ServerPayload>&);
};