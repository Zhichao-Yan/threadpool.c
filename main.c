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
    printf("处理来自%s的第%d个任务\n",p->name,p->id);
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
        product *p = (product*)malloc(sizeof(product));  // arg通常指向堆内存
        p->id = ++i;
        sprintf(p->name,"生产线程0x%x",pthread_self());
        t.arg = p;
        t.function = func;
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