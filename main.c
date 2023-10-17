#include "pool.h"
#include "task.h"
#include <stdlib.h>
#include <stdio.h>

void* func(void* arg)
{
    int id = *((int*)arg);
    printf("处理第%d个任务\n",id);
    free(arg); // 任务执行完成，释放堆分配的内存
    return NULL;
}

void* Factory(void* arg) // 生产产品的厂家
{
    pthread_detach(pthread_self());
    pool *pl = (pool*)arg;
    int i = 0;
    while(1)
    {
        task t;
        int *id = (int*)malloc(sizeof(int));  // arg通常指向堆内存
        *id = ++i;
        t.arg = id; 
        t.function = func;
        t.factory_id = pthread_self();
        Produce(pl,t);
    }
    return NULL;
}



int main(int argc,char **argv)
{
    pool *pl = pool_init(10,100,100);
    if(pl == NULL)
        exit(-1);
    if(argc != 2)
    {
        printf("pool [threads]\n");
        return -1;
    }
    int number = atoi(argv[1]);
    pthread_t *tid = (pthread_t*)malloc(sizeof(pthread_t) * number);
    for(int i = 0; i < number; ++i )
    {
        pthread_create(&tid[i],NULL,Factory,pl);
    }
    pthread_join(pl->admin,NULL);
    return 0;
}