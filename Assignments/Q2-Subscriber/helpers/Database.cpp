#include "Database.h"
#include "Time.h"
#include <iostream>
#include <functional>
#include <unordered_map>
#include <queue>
#include <vector>
#include <cassert>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <chrono>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <arpa/inet.h>
using namespace std;

void Database::removeOldMessages(const string &topic)
{
    lock();
    
    auto time = std::chrono::high_resolution_clock::now();

    while (!mp[topic].empty() &&
        std::chrono::duration<double, std::milli>(time - mp[topic].top().first).count() > MESSAGE_TIME_LIMIT * 1000)
        mp[topic].pop();

    unlock();
}

Database& Database::getInstance()
{
    static Database instance;

    return instance;
}

int Database::addTopic(const char* in_topic)
{
    if (topicExists(in_topic))
    {
        cerr << "Topic already exists!" << endl;
        return -1;
    }
    string topic(in_topic);

    lock();
    mp[topic] = {};
    unlock();
    return 0;
}

bool Database::topicExists(const char* in_topic) const
{
    return mp.find(string(in_topic)) != mp.end();
}

int Database::addMessage(const char* in_topic, const char* in_message, short_time& time)
{
    string topic(in_topic);
    string message(in_message);

    if (!topicExists(in_topic))
        return -1;

    removeOldMessages(topic);

    lock();

    mp[topic].push({ time = current_time(), message});

    unlock();
    return 0;
}

int Database::addMessages(const char* in_topic, const vector<string> &msgs, short_time& time)
{
    string topic(in_topic);
    if (!topicExists(in_topic))
        return -1;

    removeOldMessages(topic);
    lock();

    for (auto &msg: msgs)
        mp[topic].push({ time = current_time(), msg});

    unlock();
    return 0;
}

string Database::getNextMessage(const char* in_topic, short_time &clk)
{
    string topic(in_topic);
    string output = "";

    if (!topicExists(in_topic))
        return "";

    removeOldMessages(topic);

    lock();

    priority_queue<pcs> pq;

    while (!mp[topic].empty() && mp[topic].top().first <= clk)
    {
        pq.push(mp[topic].top());
        mp[topic].pop();
    }

    if (!mp[topic].empty())
    {
        output = mp[topic].top().second;
        clk = mp[topic].top().first;
    }
    
    while (!pq.empty())
    {
        mp[topic].push(pq.top());
        pq.pop();
    }

    unlock();

    return output;
}

const vector<pair<short_time, string>> Database::getBulkMessages(const char* in_topic, short_time &clk)
{
    vector<pair<short_time, string>> msgs;
    auto old = clk;

    for (int i = 0; i < BULK_LIMIT; ++i)
    {
        string s = getNextMessage(in_topic, clk);

        if (s == "")
            break;
        
        msgs.push_back({ clk, s });
    }

    return msgs;
}

const vector<string> Database::getAllTopics() const
{
    vector<string> ret;
    for (auto &x: mp)
        ret.push_back(x.first);

    return ret;
}