#include "scheduler.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "debug/debug.h"
#include "string/string.h"
#include "io/io.h"
#include "queue/queue.h"
#include "idt/idt.h"

Process* runningProcess;
Process* idleProcess;

Queue* readyProcessQueue;

void scheduler_init()
{
    tty_start_message("Scheduler initializing");
    
    readyProcessQueue = queue_new();

    idleProcess = process_kernel_new(scheduler_idle_process);
    runningProcess = idleProcess;

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_sleep(Process* process)
{
    
}

void scheduler_append(Process* process)
{
    queue_push(readyProcessQueue, process);
}

void scheduler_remove(Process* process)
{
    if (process == runningProcess)
    {
        runningProcess = idleProcess;
        return;
    }

    for (int i = 0; i < queue_length(readyProcessQueue); i++)
    {
        Process* poppedProcess = queue_pop(readyProcessQueue);    
        if (poppedProcess == process)
        {
            return;
        }       
        queue_push(readyProcessQueue, poppedProcess);
    }

    debug_panic("Failed to remove process from queue!");
}

void scheduler_schedule()
{
    if (queue_length(readyProcessQueue) == 0)
    {
        scheduler_switch(idleProcess);
        return;
    }
    else
    {
        Process* nextProcess = queue_pop(readyProcessQueue);    

        if (runningProcess != idleProcess)
        {
            runningProcess->state = PROCESS_STATE_READY;
            queue_push(readyProcessQueue, runningProcess);
        }

        scheduler_switch(nextProcess);
    }
}

void scheduler_switch(Process* process)
{
    runningProcess = process;
    runningProcess->state = PROCESS_STATE_RUNNING;
}

Process* scheduler_get_idle_process()
{
    return idleProcess;
}

Process* scheduler_running_process()
{
    if (runningProcess == 0)
    {
        debug_panic("Failed to retrieve scheduled process!");
        return 0;
    }
    else
    {
        return runningProcess;
    }
}