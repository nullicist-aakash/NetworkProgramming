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
    function<void(int)> *lambdas;

    pthread_t *threads;
    int iget, iput;

    pthread_mutex_t clifd_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t clifd_cond = PTHREAD_COND_INITIALIZER;

    static void* thread_main(void* arg);

public:
    ThreadPool(int num_threads);
    void startOperation(int &connfd, const std::function<void(int)> lambda);
    ~ThreadPool();
};
