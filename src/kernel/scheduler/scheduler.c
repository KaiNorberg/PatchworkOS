#include "scheduler.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "debug/debug.h"
#include "string/string.h"
#include "io/io.h"
#include "queue/queue.h"
#include "idt/idt.h"
#include "spin_lock/spin_lock.h"
#include "smp/smp.h"

Process* idleProcess;

Queue* readyProcessQueue;

SpinLock schedulerLock;

void scheduler_init()
{
    tty_start_message("Scheduler initializing");
    
    readyProcessQueue = queue_new();

    idleProcess = process_kernel_new(scheduler_idle_loop);

    schedulerLock = spin_lock_new();

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_acquire()
{
    spin_lock_acquire(&schedulerLock);
}

void scehduler_release()
{
    spin_lock_release(&schedulerLock);
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
    if (process == scheduler_running_process())
    {
        scheduler_switch(idleProcess);
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
        return;
    }
    else
    {
        Process* nextProcess = queue_pop(readyProcessQueue);    
        
        Process* runningProcess = scheduler_running_process();

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
    smp_current_cpu()->process = process;
    process->state = PROCESS_STATE_RUNNING;
}

Process* scheduler_idle_process()
{
    return idleProcess;
}

Process* scheduler_running_process()
{
    if (smp_current_cpu()->process == 0)
    {
        debug_panic("Failed to retrieve scheduled process!");
        return 0;
    }
    else
    {
        return smp_current_cpu()->process;
    }
}