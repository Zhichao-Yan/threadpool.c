/*
 * @Author: yan yzc53@icloud.com
 * @Date: 2023-10-17 19:42:57
 * @LastEditors: yan yzc53@icloud.com
 * @LastEditTime: 2023-10-22 08:11:45
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
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#else
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#define shutdown 0
#define running 1
#define err(str) fprintf(stderr, str)

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
    double ti; // current task wait time 
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