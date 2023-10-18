#include "pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#ifndef MIN_THREADS
#define MIN_THREADS 3 
#endif 
#define DEFAULT_INCREMENT1 2 // 线程不足核心线程数时，默认自增的线程数
#define DEFAULT_INCREMENT2 5 // 线程数量超过核心线程数时，默认的自增线程数目
#define shutdown 0
#define running 1

static volatile int threads_hold_on = 0; // 工作线程休眠控制量
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
    pl->state == shutdown;  // 把线程池状态置为shutdown关闭
    pool_queue_destroy(pl); // 执行销毁队列的工作
    free(pl->worker); // 释放为工人线程分配的空间
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
    struct sigaction act; // 数据结构struct sigaction
	sigemptyset(&act.sa_mask); // sigemptyset()清除sa_mask中的信号集
	act.sa_flags = 0;
	act.sa_handler = pool_threads_hold;
    // 调用sigaction函数修改信号SIGUSR1相关的处理动作，即以后用这个处理函数响应信号
    // 在调用信号处理函数之前act.sa_mask会被加到进程的信号屏蔽字中
    // 信号处理函数sa_handle返回后进程信号集会被还原
    // SIGUSR1 也在操作系统建立的新信号屏蔽字中，保证了在处理给定信号时，如果这种信号再次发生，它会被阻塞到信号处理完毕
    // 出错返回-1
	if (sigaction(SIGUSR1, &act, NULL) == -1) 
    { 
		printf("Work(): cannot handle SIGUSR1\n");
	}
    pool *pl = (pool*)arg;
    // 队列不为空，即使线程池shutdown，还是应该把剩余的任务处理完
    // 只有2者都不满足，才跳出循环
    while(pl->state == running || !queue_empty(&(pl->q))) // 只要队列不为空或者线程池存活就继续循环
    {
        // 存在一种情况：线程池shutdown，队列只有一个任务（非空），但是2个工作线程进入了循环
        // 一个线程取走了任务，另外一个线程阻塞在queue_pull，我们需要通过pool_queue_destroy函数中
        // 广播一个not_empty的条件变量唤醒这个线程，否则它一直阻塞在里面
        task t;
        if(queue_pull(&(pl->q),&t) == -1) // 队列状态q.state = -1 时，获取失败，返回-1，退出当前循环
            break;
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
    pthread_exit(NULL);
}

void* Admin(void* arg)
{
    pthread_detach(pthread_self());
    pool *pl = (pool*)arg;
    srand(time(NULL)); // 播下时间种子
    double busy_ratio; // 忙的线程占存活线程比例
    double queue_usage; //队列使用率
    double avg_time; // 任务平均等待时间
    while(pl->state == running)
    {
        sleep(rand()%10); // 休息随机时间后抽查运行状况

        int length = queue_length(&(pl->q));
        queue_usage = (double)length/(pl->q).size;

        // pl->alive 工作线程不需要知道有多少线程存在，管理线程知道即可
        // 因此不需要对alive互斥访问，只有管理线程知道alive
        pthread_mutex_lock(&busy_lock);
        busy_ratio = (double)pl->busy / pl->alive; 
        pthread_mutex_unlock(&busy_lock);

        pthread_mutex_lock(&time_lock);
        avg_time = pl->tawt;
        pthread_mutex_unlock(&time_lock);

        printf("目前线程池状态：\
        \n队列使用率:%f--线程使用率:%f--任务平均等待时间:%f(ms)\n",queue_usage,busy_ratio,avg_time);

        if(pl->state == running && busy_ratio <= 0.5 && pl->alive > MIN_THREADS) // 取消部分线程
        {
            int cancel = 0; // 打算取消的线程
            if(MIN_THREADS >= pl->alive * 0.5) // 如果最小线程数大于当前线程数的0.5，取消到只有最小那么多
                cancel = pl->alive - MIN_THREADS;
            else
                cancel = pl->alive * 0.5; // 如果最小线程数小于于线程数的0.5，那么直接取消一半

            thread *threads = pl->worker;
            for(int cnt = 0,j = 0; pl->state == running && cnt< cancel && j < pl->max_threads;j++)
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
        if(pl->state == running && busy_ratio >= 0.8 && queue_usage >= 0.8)
        {
            int add = 0;
            if(pl->alive < pl->core_pool_size) // 如果当前线程数小于核心线程数
                add = DEFAULT_INCREMENT1; // 以2为单位逐渐递增
            else
                add = DEFAULT_INCREMENT2; // 如果当前线程数大于核心线程数，则以5为单位递增
            thread *threads = pl->worker;
            for(int cnt = 0,j = 0; pl->state == running && cnt < add && j < pl->max_threads ;j++)
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
    pthread_exit(NULL);
}
void Produce(pool *pl,task t)
{
    queue_open_wait(&(pl->q));
    if(pl->state == running) // 如果线程池存在，那么就放入任务
        queue_push(&(pl->q),t);
    else{
        free(t.arg); // 线程池关闭了，但是该任务已经被生产，内存被分配，我们需要释放它。
    }
    return;
}


void pool_queue_pause(pool* pl)
{
    queue_pause(&(pl->q));
}
void pool_queue_resume(pool* pl)
{
    queue_resume(&(pl->q));
}

void pool_queue_destroy(pool* pl)
{
    queue_terminate(&(pl->q));
    printf("准备销毁队列...........\n");
    // 留时间让工作线程把剩余的任务处理完
    while(!queue_empty(&(pl->q))) // 队列不为空，等待工作线程把剩余任务取走然后完成
        sleep(1);
    // 队列为空，可能存在工作线程阻塞等待（队列为非空）的条件状态
    // 但是此时生产线程已经不再生产任务，队列不再可能非空，但是我们还是应该广播唤醒等待的工作线程
    // 队列的此时状态q.state = -1，利用它唤醒
    pthread_cond_broadcast(&(pl->q).not_empty);
    queue_destroy(&(pl->q)); // 销毁队列
    return;
}


// 信号处理函数
void pool_threads_hold(int signal) 
{
    if(signal == SIGUSR1)
	{
		threads_hold_on = 1;
		while (threads_hold_on){
			sleep(1); // 线程循环休眠，直到pool_thread_resume将threads_hold_on置0，线程不在休眠
		}
	}
	return;
}
// 所有工作线程休眠
void pool_threads_pause(pool* pl)
{
	for ( int i = 0; i < pl->max_threads; i++){
        if(pl->worker[i].state == 1)
		    pthread_kill(pl->worker[i].tid, SIGUSR1);
	}
}
// 所有线程重新醒过来
void pool_threads_resume() 
{
    threads_hold_on = 0; 
}
