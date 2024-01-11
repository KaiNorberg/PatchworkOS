#include "scheduler.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "debug/debug.h"
#include "string/string.h"
#include "io/io.h"
#include "idt/idt.h"
#include "smp/smp.h"
#include "time/time.h"

Scheduler* schedulers[SMP_MAX_CPU_AMOUNT];

void scheduler_init()
{
    tty_start_message("Scheduler initializing");
    memclr(schedulers, sizeof(Scheduler*) * SMP_MAX_CPU_AMOUNT);

    for (int i = 0; i < SMP_MAX_CPU_AMOUNT; i++)
    {
        if (smp_cpu(i)->present)
        {
            schedulers[i] = kmalloc(sizeof(Scheduler));

            schedulers[i]->readyQueue = queue_new();
            schedulers[i]->runningProcess = 0;
            schedulers[i]->lock = spin_lock_new();
        }
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_schedule(InterruptFrame* interruptFrame)
{

}

void scheduler_acquire()
{

}

void scheduler_release()
{

}

void scheduler_push(Process* process)
{

}