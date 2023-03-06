#pragma once

#include <pthread.h>
#include <functional>
#include <iostream>
#include <unistd.h>

using namespace std;

class ThreadPool
{
private:
    const int nthreads;
    int *clifd;
    function<void(int, int)> *lambdas;
    pthread_mutex_t *thread_mutexes;
    pthread_cond_t *thread_conds;

    pthread_t *threads;
    int iget, iput;

    pthread_mutex_t clifd_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t clifd_cond = PTHREAD_COND_INITIALIZER;

    static void* thread_main(void* arg);

public:
    ThreadPool(int num_threads);
    void startOperation(const int &connfd, const std::function<void(int, int)> lambda);

    pthread_mutex_t& getMutexFromThreadID(int);
    pthread_cond_t& getCondFromThreadID(int);
    int capacity() const;

    ~ThreadPool();
};
