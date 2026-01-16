#include <kernel/sync/tasks.h>

bool task_nop_cancel(task_nop_t* task)
{
    TASK_COMPLETE(task, 0);
    return true;
}

void task_nop_timeout(task_nop_t* task)
{
    TASK_COMPLETE(task, 0);
}