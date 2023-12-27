#include "scheduler.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "debug/debug.h"
#include "string/string.h"
#include "io/io.h"
#include "queue/queue.h"
#include "tss/tss.h"

Process* runningProcess;

Queue* readyProcessQueue;

void scheduler_init()
{
    tty_start_message("Scheduler initializing");
    
    readyProcessQueue = queue_new();

    runningProcess = 0;

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
        runningProcess = 0;
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
    Process* nextProcess = queue_pop(readyProcessQueue);    
    if (nextProcess != 0)
    {        
        if (runningProcess != 0)
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

Process* scheduler_get_running_process()
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

void scheduler_yield_to_user_space()
{
    scheduler_schedule();
    
    io_pic_clear_mask(IRQ_PIT);

    jump_to_user_space((void*)runningProcess->interruptFrame->instructionPointer, (void*)runningProcess->interruptFrame->stackPointer, (void*)runningProcess->interruptFrame->cr3);
}