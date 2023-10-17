#include "task.h"

void execute_task(task t)
{
    (t.function)(t.arg);
    return;
}