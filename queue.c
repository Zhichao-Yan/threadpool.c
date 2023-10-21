/*
 * @Author: yan yzc53@icloud.com
 * @Date: 2023-10-16 16:06:07
 * @LastEditors: yan yzc53@icloud.com
 * @LastEditTime: 2023-10-21 21:25:40
 * @FilePath: /threadpool.c/queue.c
 * @Description:
 *
 * Copyright (c) 2023 by ${git_name_email}, All Rights Reserved.
 */
#include "queue.h"
#include "time.h"
#include <stdio.h>
#include <stdlib.h>

int queue_init(queue *q, int size)
{
    q->tq = (task *)malloc(sizeof(task) * size);
    if (q->tq == NULL)
    {
        fprintf(stderr,"Failed to malloc a queue\n");
        return -1;
    }
    q->front = 0;
    q->rear = 0;
    q->len = 0;
    q->size = size;
    q->state = 1;
    pthread_cond_init(&(q->state_cond), NULL);
    pthread_cond_init(&(q->not_empty), NULL);
    pthread_cond_init(&(q->not_full), NULL);
    pthread_mutex_init(&(q->mutex), NULL);
    return 0;
}

int queue_empty(queue *q)
{
    if (q->front == q->rear)
        return 1;
    return 0;
}
int queue_full(queue *q)
{
    if ((q->rear + 1) % (q->size) == q->front)
        return 1;
    return 0;
}

void queue_push(queue *q, task t)
{
    pthread_mutex_lock(&(q->mutex));
    while (queue_full(q))
    {
        pthread_cond_wait(&(q->not_full), &(q->mutex));
    }
    t.ti = clock();
    q->tq[q->rear] = t;
    ++q->len;
    q->rear = (q->rear + 1) % q->size;
    pthread_cond_signal(&(q->not_empty));
    pthread_mutex_unlock(&(q->mutex));
    return;
}
int queue_pull(queue *q, task *re)
{
    do
    {
        pthread_mutex_lock(&(q->mutex)); // 加锁
        while (queue_empty(q) && q->state >= 0)
        {
            pthread_cond_wait(&(q->not_empty), &(q->mutex));
        }
        if (q->state == -1)
        {
            pthread_mutex_unlock(&(q->mutex)); // 退出前解锁
            return -1;
        }
        *re = q->tq[q->front];           // 任务数据被拷贝走，可以置空了
        q->tq[q->front].arg = NULL;      // 把取走的任务数据指针置空
        q->tq[q->front].function = NULL; // 把取走的任务函数指针置空
        q->front = (q->front + 1) % q->size;
        --q->len;
        pthread_cond_signal(&(q->not_full));
    } while (0);
    pthread_mutex_unlock(&(q->mutex)); // 退出前解锁
    return 1;
}

void queue_destroy(queue *q)
{
    if (q) // free it if q.queue is not NULL
    {
        free(q->tq);
        q->tq = NULL;
        q->size = 0;
        q->len = 0;
        q->front = q->rear = 0;
        q->state = -1; // 队列处在销毁状态
        pthread_cond_destroy(&(q->not_full));
        pthread_cond_destroy(&(q->not_empty));
        pthread_cond_destroy(&(q->state_cond));
        pthread_mutex_destroy(&(q->mutex));
    }
    // do nothing if the q.queue is NULL
    return;
}

int queue_length(queue *q)
{
    pthread_mutex_lock(&(q->mutex));
    int length = q->len;
    pthread_mutex_unlock(&(q->mutex));
    return length;
}
int queue_state(queue *q)
{
    pthread_mutex_lock(&q->mutex);
    int state = q->state;
    pthread_mutex_unlock(&q->mutex);
    return state;
}

// 队列暂停，暂时不接受任务
void queue_pause(queue *q)
{
    pthread_mutex_lock(&q->mutex);
    if (q->state == 1) // 队列目前是打开的
        q->state = 0;  // 那么我们把它关闭
    pthread_mutex_unlock(&q->mutex);
}

// 通知所有生产线程可队列开始接收任务
void queue_resume(queue *q)
{
    pthread_mutex_lock(&q->mutex);
    if (q->state == 0) // 队列目前是关闭的
    {
        q->state = 1;                           // 那么我们把它打开
        pthread_cond_broadcast(&q->state_cond); // 通知所有生产线程
    }
    pthread_mutex_unlock(&q->mutex);
}

void queue_wakeup_factory(queue *q)
{
    if(q->state == 0) // 当q->state == 0，factory线程会阻塞在queue_open_wait中
    {
        q->state = 1;                          // 队列停止使用
        pthread_cond_broadcast(&q->state_cond); // 通知所有生产线程      
    }
}

void queue_open_wait(queue *q)
{
    pthread_mutex_lock(&q->mutex);
    while (q->state == 0)
    { // state == 0，队列关闭，不再接受任务,生产线程阻塞在这
        pthread_cond_wait(&q->state_cond, &q->mutex);
    }
    pthread_mutex_unlock(&q->mutex);
}
