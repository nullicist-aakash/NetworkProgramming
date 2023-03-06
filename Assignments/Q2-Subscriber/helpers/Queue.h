#pragma once

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

template <class T>
class Queue
{
private:
    int qid;

    struct my_msg
    {
        long mtype;
        T data;
    };

public:
    Queue(key_t id)
    {
        if (id == -1)
        {
            perror("ftok");
            exit(-1);
        }

        qid = msgget(id, IPC_CREAT | 0660);

        if (qid == -1)
        {
            perror("msgget");
            exit(-1);
        }
    }

    void send_data(int type, const T data, const char* errMsg = nullptr) const
    {
        my_msg msg { type, data };

        if (msgsnd(qid, &msg, sizeof(msg) - sizeof(long), 0) == -1)
        {
            perror(errMsg == nullptr ? "msgsnd" : errMsg);
            exit(-1);
        }
    }

    T receive_data(int type, const char* errMsg = nullptr) const
    {
        my_msg msg;
        
        if (msgrcv(qid, &msg, sizeof(my_msg) - sizeof(long), type, 0) == -1)
        {
            perror(errMsg == nullptr ? "msgrcv" : errMsg);
            exit(-1);
        }

        return msg.data;
    }

    ~Queue()
    {
        if (msgctl(qid, IPC_RMID, nullptr) == -1)
                perror("msgctl");
    }
};
