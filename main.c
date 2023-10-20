/*
 * @Author: yan yzc53@icloud.com
 * @Date: 2023-10-17 19:42:57
 * @LastEditors: yan yzc53@icloud.com
 * @LastEditTime: 2023-10-20 17:18:54
 * @FilePath: /threadpool.c/main.c
 * @Description: 
 * 
 * Copyright (c) 2023 by ${git_name_email}, All Rights Reserved. 
 */
#include <stdlib.h>
#include <stdio.h>
#include "pool.h"
#include "task.h"


typedef struct {
    int id;
    char name[50];
}product;

void* func(void* arg)
{
    product *p = (product*)arg;
    fprintf(stdout,"处理来自%s的第%d个任务\n",p->name,p->id);
    free(arg); // 任务执行完成，释放堆分配的内存
    return NULL;
}

void* Factory(void* arg) // 生产产品的厂家
{
    char thread_name[] = "Factory";
#if defined(__linux__)
	/* Use prctl instead to prevent using _GNU_SOURCE flag and implicit declaration */
	prctl(PR_SET_NAME, thread_name);
#elif defined(__APPLE__) && defined(__MACH__)
	pthread_setname_np(thread_name);
#endif

    pthread_detach(pthread_self()); //TODO
    pool *pl = (pool*)arg;
    int i = 0;
    while(pl->state == running) // 线程池在运行才继续生产
    {
        task t;
        product *p = (product*)malloc(sizeof(product));  // arg通常指向堆内存
        p->id = ++i;
#if defined(__APPLE__)
        sprintf(p->name,"生产线程0x%p",pthread_self());
#elif defined(__linux__)
        sprintf(p->name,"生产线程%ld",pthread_self());
#endif
        t.arg = p;
        t.function = func;
        Produce(pl,t);
    }
    pthread_exit(NULL);
}



int main(int argc,char **argv)
{
    pool *pl = pool_init(10,100,100);
    if(pl == NULL)
        exit(-1);
    if(argc != 2)
    {
        err("pool [threads]\n");
        return -1;
    }
    int number = atoi(argv[1]);
    pthread_t *tid = (pthread_t*)malloc(sizeof(pthread_t) * number);
    for(int i = 0; i < number; ++i )
    {
        pthread_create(&tid[i],NULL,Factory,pl);
    }
    sleep(15);
    pool_threads_pause(pl);
    sleep(50);
    pool_threads_resume();
    sleep(15);
    pool_queue_pause(pl);
    sleep(50);
    pool_queue_resume(pl);
    sleep(15);
    pthread_join(pl->admin,NULL);
    return 0;
}