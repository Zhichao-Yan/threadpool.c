#ifndef POOL_H
#define POOL_H
#include "task.h"
#include "queue.h"
#include <pthread.h>
#include <unistd.h>
#include <time.h>

typedef struct thread_t
{
    pthread_t tid;
    int state;
}thread;

typedef struct pool_t{
    // 线程池组件
    queue q;
    pthread_t admin;
    thread *worker;

    // pool参数
    int core_pool_size;
    int min_threads;
    int max_threads;

    // 监控数据
    int state; // 线程池状态
    int alive;
    int busy;
    double tawt; // task average wait time in queue
}pool;

pool* pool_init(int core_pool_size,int max_threads,int max_queue);
void pool_destroy(pool *pl);
void* Admin(void* arg);
void* Work(void* arg);
void Produce(pool *pl,task t);
#endif