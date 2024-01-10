#include "scheduler.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "debug/debug.h"
#include "string/string.h"
#include "io/io.h"
#include "queue/queue.h"
#include "idt/idt.h"
#include "smp/smp.h"
#include "spin_lock/spin_lock.h"
#include "time/time.h"

Process* idleProcess;
Process* runningArray[SMP_MAX_CPU_AMOUNT];

Queue* readyQueue;

SpinLock schedulerLock;

void scheduler_init()
{
    tty_start_message("Scheduler initializing");
    
    readyQueue = queue_new();

    idleProcess = process_kernel_new(scheduler_idle_loop);

    schedulerLock = spin_lock_new();

    for (uint64_t i = 0; i < SMP_MAX_CPU_AMOUNT; i++)
    {
        runningArray[i] = idleProcess;
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_tick(InterruptFrame* interruptFrame)
{
    for (uint64_t cpuId = 0; cpuId < smp_cpu_amount(); cpuId++)
    {
        Cpu* cpu = smp_cpu(cpuId);

        if (runningArray[cpu->id]->timeEnd <= time_nanoseconds()) 
        {   
            if (cpu == smp_current_cpu())
            {
                scheduler_yield(interruptFrame);
            }
            else
            {
                Ipi ipi = IPI_CREATE(IPI_TYPE_YIELD);
                smp_send_ipi(cpu, ipi);
            }
        }
    }
}

void scheduler_exit()
{        
    Cpu* cpu = smp_current_cpu();

    if (runningArray[cpu->id] != idleProcess)
    {
        process_free(runningArray[cpu->id]);
    }
    runningArray[cpu->id] = idleProcess;
}

void scheduler_acquire()
{
    spin_lock_acquire(&schedulerLock);
}

void scheduler_release()
{
    spin_lock_release(&schedulerLock);
}

void scheduler_yield(InterruptFrame* interruptFrame)
{    
    Cpu* cpu = smp_current_cpu();

    interrupt_frame_copy(runningArray[cpu->id]->interruptFrame, interruptFrame);
    scheduler_schedule();    
    interrupt_frame_copy(interruptFrame, runningArray[cpu->id]->interruptFrame);
}

void scheduler_append(Process* process)
{
    queue_push(readyQueue, process);
}

void scheduler_remove(Process* process)
{        
    Process* runningProcess = scheduler_running_process();

    if (process == runningProcess)
    {
        scheduler_switch(idleProcess);
        return;
    }

    for (int i = 0; i < queue_length(readyQueue); i++)
    {
        Process* poppedProcess = queue_pop(readyQueue);    
        if (poppedProcess == process)
        {
            return;
        }       
        queue_push(readyQueue, poppedProcess);
    }

    debug_panic("Failed to remove process from queue!");
}

void scheduler_schedule()
{
    if (queue_length(readyQueue) == 0)
    {
        scheduler_switch(idleProcess);
        return;
    }
    else
    {
        Process* nextProcess = queue_pop(readyQueue);    

        Cpu* cpu = smp_current_cpu();

        if (runningArray[cpu->id] != idleProcess)
        {
            runningArray[cpu->id]->state = PROCESS_STATE_READY;
            queue_push(readyQueue, runningArray[cpu->id]);
        }

        scheduler_switch(nextProcess);
    }
}

Process* scheduler_running_process()
{
    return runningArray[smp_current_cpu()->id];
}

void scheduler_switch(Process* process)
{
    runningArray[smp_current_cpu()->id] = process;
    process->timeStart = time_nanoseconds();
    process->timeEnd = process->timeStart + NANOSECONDS_PER_SECOND / 2;
    process->state = PROCESS_STATE_RUNNING;
}