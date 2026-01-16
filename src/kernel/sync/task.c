#include <kernel/sched/clock.h>
#include <kernel/sync/task.h>

#include <kernel/cpu/percpu.h>

PERCPU_DEFINE_CTOR(task_ctx_t, pcpu_tasks)
{
    task_ctx_t* tasks = SELF_PTR(pcpu_tasks);

    list_init(&tasks->timeouts);
    lock_init(&tasks->lock);
}

void task_timeout_add(task_t* task)
{
    task_ctx_t* tasks = SELF_PTR(pcpu_tasks);
    LOCK_SCOPE(&tasks->lock);

    task->owner = tasks;

    task_t* entry;
    LIST_FOR_EACH(entry, &tasks->timeouts, timeoutEntry)
    {
        if (task->deadline < entry->deadline)
        {
            list_prepend(&entry->entry, &task->timeoutEntry);
            return;
        }
    }

    list_push_back(&tasks->timeouts, &task->timeoutEntry);
}

void task_timeout_remove(task_t* task)
{
    assert(task->owner != NULL);
    LOCK_SCOPE(&task->owner->lock);

    list_remove(&task->timeoutEntry);
}

void task_timeouts_check(void)
{
    task_ctx_t* tasks = SELF_PTR(pcpu_tasks);
    clock_t now = clock_uptime();

    LOCK_SCOPE(&tasks->lock);

    task_t* task;
    while (true)
    {
        task = CONTAINER_OF(list_first(&tasks->timeouts), task_t, timeoutEntry);
        if (task == NULL)
        {
            break;
        }

        if (task->deadline > now)
        {
            break;
        }

        list_remove(&task->timeoutEntry);
        /// @todo 
    }
}