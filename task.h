#ifndef TASK_H
#define TASK_H
#include <time.h>
#include <pthread.h>

typedef struct task{
    void* (*function)(void*); // function used for task processing
    void* arg; // input data for task 
    clock_t ti; // the clock_t when the task enter the queue
}task;

void execute_task(task t);
#endif