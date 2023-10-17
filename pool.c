#include "pool.h"
#include <stdlib.h>
#include <stdio.h>

#ifndef MIN_THREADS
#define MIN_THREADS 3 
#endif 
#define DEFAULT_INCREMENT1 2 // 线程不足核心线程数时，默认自增的线程数
#define DEFAULT_INCREMENT2 5 // 线程数量超过核心线程数时，默认的自增线程数目
#define shutdown 0
#define running 1


pthread_mutex_t busy_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t time_lock = PTHREAD_MUTEX_INITIALIZER;

static double get_avg_time(double ck)
{
    static double t[5] = {-1.0,-1.0,-1.0,-1.0,-1.0};
    static int i = 0;
    static double sum = 0.0;
    double avg;
    if(t[i] < 0)
    {
        t[i] = ck;
        sum += ck;
        avg = sum / (i+1);
    }else{
        sum = sum - t[i] + ck;
        avg = sum / 5;
        t[i] = ck;
    }
    i = (i + 1) % 5;
    return avg;
}

pool* pool_init(int core_pool_size,int max_threads,int max_queue)
{
    pool * pl = (pool*)malloc(sizeof(pool));
    if(pl == NULL)
    {
        printf("Failed to initialzie a pool!\n");
        return NULL;
    }
    pl->min_threads = MIN_THREADS; // 最小线程数
    pl->core_pool_size = core_pool_size; // 核心线程数
    pl->max_threads = max_threads; // 最大线程数
    pl->busy = 0;
    pl->alive = MIN_THREADS;
    pl->state = running;
    do{
        if(queue_init(&(pl->q),max_queue) != 0)
        {
            printf("Failed to initialzie a queue!\n");
            free(pl);
            pl = NULL;
            break;
        }
        pl->worker = (thread*)malloc(sizeof(thread) * max_threads);
        if(pl->worker == NULL)
        {
            printf("Failed to malloc memory for worker!\n");
            queue_destroy(&(pl->q));
            free(pl);
            pl = NULL;
            break;
        }
        thread *threads = pl->worker;
        for(int i = 0; i < MIN_THREADS; ++i)
        {
            pthread_create(&(threads[i].tid),NULL,Work,(void*)pl);
            threads[i].state = 1;
        }
        pthread_create(&(pl->admin),NULL,Admin,(void*)pl);
    }while(0);
    return pl;
}
void pool_destroy(pool *pl)
{
    queue_destroy(&(pl->q));
    free(pl->worker);
    free(pl);
    pl = NULL;
}
void clean(void *arg)
{
    pool *pl = (pool*)arg;
    pthread_mutex_unlock(&((pl->q).mutex)); // 清理函数，线程响应取消时，锁可能没释放，因此清理函数应当释放锁
    printf("thread:%ld响应取消退出\n",pthread_self());
    return;
}
void* Work(void* arg)
{
    pthread_detach(pthread_self());
    pthread_cleanup_push(clean,arg); // 注册清理函数，线程响应取消时执行清理函数
    pool *pl = (pool*)arg;
    while(pl->state)
    {
        task t = queue_pull(&(pl->q));
        double ck = (double)(clock() - t.ti)/CLOCKS_PER_SEC * 1000;
        double avg = get_avg_time(ck);

        pthread_mutex_lock(&time_lock);
        pl->tawt = avg;
        pthread_mutex_unlock(&time_lock);

        pthread_mutex_lock(&busy_lock);
        ++pl->busy;
        pthread_mutex_unlock(&busy_lock);

        execute_task(t);

        pthread_mutex_lock(&busy_lock);
        --pl->busy;
        pthread_mutex_unlock(&busy_lock);
        pthread_testcancel(); // 取消点
    }
    pthread_cleanup_pop(0); // 弹出清理函数，参数为0，正常运行结束不会执行清理函数
    return NULL;
}
void Produce(pool *pl,task t)
{
    queue_push(&(pl->q),t);
    return;
}
void* Admin(void* arg)
{
    pool *pl = (pool*)arg;
    srand(time(NULL)); // 播下时间种子
    double busy_ratio; // 忙的线程占存活线程比例
    double queue_usage; //队列使用率
    double avg_time; // 任务平均等待时间
    while(pl->state)
    {
        sleep(rand()%10); // 休息随机时间后抽查运行状况

        int length = queue_length(&(pl->q));
        queue_usage = (double)length/(pl->q).size;

        pthread_mutex_lock(&busy_lock);
        busy_ratio = (double)pl->busy / pl->alive;
        pthread_mutex_unlock(&busy_lock);

        pthread_mutex_lock(&time_lock);
        avg_time = pl->tawt;
        pthread_mutex_unlock(&time_lock);

        printf("目前线程池状态：\
        \n队列使用率:%f--线程使用率:%f--任务平均等待时间:%f(ms)\n",queue_usage,busy_ratio,avg_time);

        if(busy_ratio <= 0.5 && pl->alive > MIN_THREADS) // 取消部分线程
        {
            int cancel = 0; // 打算取消的线程
            if(MIN_THREADS >= pl->alive * 0.5) // 如果最小线程数大于当前线程数的0.5，取消到只有最小那么多
                cancel = pl->alive - MIN_THREADS;
            else
                cancel = pl->alive * 0.5; // 如果最小线程数小于于线程数的0.5，那么直接取消一半

            thread *threads = pl->worker;
            for(int cnt = 0,j = 0; cnt< cancel && j < pl->max_threads;j++)
            {
                if(threads[j].state == 1)
                {
                    pthread_cancel(threads[j].tid); // 给线程发送取消信号
                    //printf("取消线程%ld\n",threads[j].tid);
                    --pl->alive;
                    threads[j].state = 0;
                    ++cnt;
                }
            }
        }
        if(busy_ratio >= 0.8 && queue_usage >= 0.8)
        {
            int add = 0;
            if(pl->alive < pl->core_pool_size) // 如果当前线程数小于核心线程数
                add = DEFAULT_INCREMENT1; // 以2为单位逐渐递增
            else
                add = DEFAULT_INCREMENT2; // 如果当前线程数大于核心线程数，则以5为单位递增
            thread *threads = pl->worker;
            for(int cnt = 0,j = 0; cnt < add && j < pl->max_threads ;j++)
            {
                if(threads[j].state == 0) 
                {
                    pthread_create(&(threads[j].tid),NULL,Work,(void*)pl); // 创建新线程
                    //printf("新增线程%ld\n",threads[j].tid);
		            ++pl->alive;
                    threads[j].state = 1;
                    ++cnt;
                }
            }
        }
    }
}


