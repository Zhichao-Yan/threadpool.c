/*
 * @Author: yan yzc53@icloud.com
 * @Date: 2023-10-17 19:42:57
 * @LastEditors: yan yzc53@icloud.com
 * @LastEditTime: 2023-10-19 12:26:26
 * @FilePath: /threadpool.c/pool.h
 * @Description: 
 * 
 * Copyright (c) 2023 by ${git_name_email}, All Rights Reserved. 
 */
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

void pool_threads_hold(int signal);
void pool_threads_pause(pool* pl);
void pool_threads_resume();

void pool_queue_pause(pool* pl);
void pool_queue_resume(pool* pl);
void pool_queue_destroy(pool* pl);

#endif