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
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <arpa/inet.h>
using namespace std;


#define MESSAGE_TIME_LIMIT 60
#define BULK_LIMIT 10

class Database
{
private:
    using pcs = pair<clock_t, string>;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    unordered_map<string, priority_queue<pcs, vector<pcs>, greater<pcs>>> mp;

    inline void lock()
    {
        pthread_mutex_lock(&mutex);
    }

    inline void unlock()
    {
        pthread_mutex_unlock(&mutex);
    }

    void removeOldMessages(const string &topic, const clock_t &time)
    {
        lock();

        while (!mp[topic].empty() && ((time - mp[topic].top().first) / CLOCKS_PER_SEC) > MESSAGE_TIME_LIMIT)
            mp[topic].pop();

        unlock();
    }

    Database() {}

public:
    Database(Database const&)        = delete;
    void operator=(Database const&)  = delete;

    static Database& getInstance()
    {
        static Database instance;

        return instance;
    }

    int addTopic(const char* in_topic)
    {
        if (topicExists(in_topic))
        {
            cout << "Topic already exists!" << endl;
            return -1;
        }
        string topic(in_topic);

        lock();
        mp[topic] = {};
        unlock();
        return 0;
    }

    inline bool topicExists(const char* in_topic) const
    {
        string topic(in_topic);
        return mp.find(topic) != mp.end();
    }

    int addMessage(const char* in_topic, const char* in_message)
    {
        string topic(in_topic);
        string message(in_message);

        if (!topicExists(in_topic))
            return -1;

        clock_t time = clock();
        removeOldMessages(topic, time);

        lock();

        mp[topic].push({time, message});

        unlock();
        return 0;
    }

    int addMessages(const char* in_topic, const vector<string> &msgs)
    {
        string topic(in_topic);
        if (!topicExists(in_topic))
            return -1;

        clock_t time = clock();
        removeOldMessages(topic, time);
        lock();

        for (auto &msg: msgs)
            mp[topic].push({time, msg});

        unlock();
        return 0;
    }

    const int getMessage(const char* in_topic, clock_t &clk, string &output)
    {
        string topic(in_topic);
        output = "";

        if (!topicExists(in_topic))
            return -1;

        clock_t time = clock();
        removeOldMessages(topic, time);

        lock();

        priority_queue<pcs> pq;
        while (!mp[topic].empty() && mp[topic].top().first < clk)
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

        if (output == "")
            return -1;
        
        return 0;
    }

    const vector<string> getBulkMessages(const char* in_topic)
    {
        if (!topicExists(in_topic))
            return {};
        
        removeOldMessages(in_topic, clock());

        lock();
        string topic(in_topic);

        vector<pcs> temp;
        for (int i = 0; i < BULK_LIMIT; ++i)
        {
            if (mp[topic].empty())
                break;
            
            temp.push_back(mp[topic].top());
            mp[topic].pop();
        }

        vector<string> ans;
        for (auto &x: temp)
        {
            ans.push_back(x.second);
            mp[topic].push(x);
        }

        unlock();

        return ans;
    }

    const vector<string> getAllTopics() const
    {
        vector<string> ret;
        for (auto &[key, val]: mp)
            ret.push_back(key);

        return ret;
    }
};

