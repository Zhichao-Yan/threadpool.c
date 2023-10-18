#include "queue.h"
#include "time.h"
#include <stdio.h>
#include <stdlib.h>

int queue_init(queue *q,int size)
{
    q->tq = (task*)malloc(sizeof(task) * size);
    if(q->tq == NULL)
    {
        printf("Failed to malloc a queue\n");
        return -1;
    }
    q->front = 0;
    q->rear = 0;
    q->len = 0;
    q->size = size;
    pthread_cond_init(&(q->not_empty),NULL);
    pthread_cond_init(&(q->not_full),NULL);
    pthread_mutex_init(&(q->mutex),NULL);
    return 0;
}
void queue_push(queue *q,task t)
{   
    pthread_mutex_lock(&(q->mutex));
    while(queue_full(q)){
        pthread_cond_wait(&(q->not_full),&(q->mutex));
    }
    t.ti = clock();
    q->tq[q->rear] = t;
    ++q->len;
    q->rear = (q->rear + 1) % q->size;
    pthread_cond_signal(&(q->not_empty));
    pthread_mutex_unlock(&(q->mutex));
    return;
}
int queue_empty(queue *q)
{
    if(q->front == q->rear)
        return 1;
    return 0;
}
int queue_full(queue *q)
{
    if((q->rear + 1)%(q->size) == q->front)
        return 1;
    return 0;
}

task queue_pull(queue *q)
{
    pthread_mutex_lock(&(q->mutex));
    while(queue_empty(q)){
        pthread_cond_wait(&(q->not_empty),&(q->mutex));
    }
    task re = q->tq[q->front]; // 任务数据被拷贝走，可以置空了
    q->tq[q->front].arg = NULL; // 把取走的任务数据指针置空
    q->tq[q->front].function = NULL; // 把取走的任务函数指针置空
    q->front = (q->front + 1) % q->size;
    --q->len;
    pthread_cond_signal(&(q->not_full));
    pthread_mutex_unlock(&(q->mutex));
    return re;
}

void queue_destroy(queue *q)
{
    if(q)  // free it if q.queue is not NULL
    {
        free(q->tq);
        q->tq = NULL;
        q->size = 0;
        q->len = 0;
        q->front = q->rear = 0;
        pthread_cond_destroy(&(q->not_full));
        pthread_cond_destroy(&(q->not_empty));
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