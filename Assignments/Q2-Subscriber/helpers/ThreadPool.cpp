#include "ThreadPool.h"
#include <pthread.h>
#include <functional>
#include <iostream>
#include <unistd.h>

using namespace std;

void* ThreadPool::thread_main(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;
    
    while (1)
    {
        pthread_mutex_lock(&pool->clifd_mutex);

        while (pool->iget == pool->iput)
            pthread_cond_wait(&pool->clifd_cond, &pool->clifd_mutex);
        
        int index = pool->iget;

        if (++pool->iget == pool->nthreads)
            pool->iget = 0;

        pthread_mutex_unlock(&pool->clifd_mutex);

        int connfd = pool->clifd[index];

        pool->lambdas[index](connfd, index);
        close(connfd);
    }
}

ThreadPool::ThreadPool(int num_threads) : nthreads {num_threads}
{
    iget = iput = 0;
    threads = new pthread_t[num_threads];
    clifd = new int[num_threads];
    lambdas = new function<void(int, int)>[num_threads];
    thread_mutexes = new pthread_mutex_t[num_threads];
    thread_conds = new pthread_cond_t[num_threads];
    
    for (int i = 0; i < num_threads; ++i)
    {
        lambdas[i] = nullptr;
        thread_mutexes[i] = PTHREAD_MUTEX_INITIALIZER;
        thread_conds[i] = PTHREAD_COND_INITIALIZER;
    }

    for (int i = 0; i < num_threads; ++i)
        pthread_create(&threads[i], NULL, &thread_main, (void*)this);
}

void ThreadPool::startOperation(const int &connfd, const std::function<void(int, int)> lambda)
{
    pthread_mutex_lock(&clifd_mutex);
    clifd[iput] = connfd;
    lambdas[iput] = lambda;
    
    if (++iput == nthreads)
        iput = 0;
    
    if (iput == iget)
    {
        cerr << "iput = iget = " << iput << endl;
        exit(1);
    }

    pthread_cond_signal(&clifd_cond);
    pthread_mutex_unlock(&clifd_mutex);
}

pthread_mutex_t& ThreadPool::getMutexFromThreadID(int index)
{
    return thread_mutexes[index];
}

pthread_cond_t& ThreadPool::getCondFromThreadID(int index)
{
    return thread_conds[index];
}

int ThreadPool::capacity() const
{
    return nthreads;
}

ThreadPool::~ThreadPool()
{
    delete[] clifd;
    delete[] threads;
    delete[] thread_mutexes;
    delete[] thread_conds;
}