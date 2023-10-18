#ifndef QUEUE_H
#define QUEUE_H
#include "task.h"
#include "pthread.h"

// 循环队列
typedef struct queue_t
{
    task* tq;
    int front; // 指向队头的第一个任务
    int rear; // 指向任务队列队尾的可以存放任务的下一个空位置
    int len; // 任务队列实时长度
    int size; // 任务队列大小
    int state; // state = <0 队列销毁 |state = 0 队列暂停 |state > 0 队列启动｜
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    pthread_cond_t state_cond;
    pthread_mutex_t mutex;
}queue;

int queue_init(queue *q,int size);
int queue_empty(queue *q);
int queue_full(queue *q);
void queue_push(queue *q,task t);
task queue_pull(queue *q);
void queue_destroy(queue *q);
int queue_length(queue *q);
int queue_state(queue *q);
void queue_pause(queue *q);
void queue_resume(queue *q);
void queue_open_wait(queue *q);
#endif