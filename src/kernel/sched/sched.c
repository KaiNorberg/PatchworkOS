#include "sched.h"

#include <string.h>

#include "vmm/vmm.h"
#include "smp/smp.h"
#include "heap/heap.h"
#include "gdt/gdt.h"
#include "time/time.h"
#include "debug/debug.h"
#include "regs/regs.h"
#include "irq/irq.h"
#include "apic/apic.h"
#include "hpet/hpet.h"
#include "loader/loader.h"
#include "sched/schedule/schedule.h"

static void sched_spawn_init_thread(void)
{
    Process* process = process_new(0);
    Thread* thread = thread_new(process, 0, THREAD_PRIORITY_MAX);
    thread->timeEnd = UINT64_MAX;

    smp_self_unsafe()->scheduler.runningThread = thread;
}

void scheduler_init(Scheduler* scheduler)
{
    for (uint64_t p = THREAD_PRIORITY_MIN; p <= THREAD_PRIORITY_MAX; p++)
    {
        scheduler->queues[p] = queue_new();
    }
    scheduler->killedThreads = queue_new();
    scheduler->blockedThreads = array_new();
    scheduler->runningThread = NULL;
}

void sched_start(void)
{
    tty_start_message("Scheduler starting");

    sched_spawn_init_thread();

    smp_send_ipi_to_others(IPI_START);
    sched_cpu_start();

    tty_end_message(TTY_MESSAGE_OK);
}

void sched_cpu_start(void)
{
    apic_timer_init(IPI_BASE + IPI_SCHEDULE, CONFIG_SCHED_HZ);
}

Thread* sched_thread(void)
{
    Thread* thread = smp_self()->scheduler.runningThread;
    smp_put();
    return thread;
}

Process* sched_process(void)
{
    Process* process = smp_self()->scheduler.runningThread->process;
    smp_put();
    return process;
}

void sched_yield(void)
{
    SMP_SEND_IPI_TO_SELF(IPI_SCHEDULE);
}

static bool sched_sleep_callback(uint64_t deadline)
{
    return deadline <= time_nanoseconds();
}

void sched_sleep(uint64_t nanoseconds)
{
    Blocker blocker =
    {
        .context = time_nanoseconds() + nanoseconds,
        .callback = sched_sleep_callback
    };
    sched_block(blocker);
}

void sched_block(Blocker blocker)
{
    if (blocker.callback(blocker.context))
    {
        return;
    }

    Scheduler* scheduler = &smp_self()->scheduler;
    Thread* thread = scheduler->runningThread;
    thread->blocker = blocker;
    thread->state = THREAD_STATE_BLOCK_GUARDED;
    smp_put();

    sched_yield();
}

void sched_process_exit(uint64_t status)
{
    //TODO: Add handling for status

    Scheduler* scheduler = &smp_self()->scheduler;
    scheduler->runningThread->state = THREAD_STATE_KILLED;
    scheduler->runningThread->process->killed = true;
    smp_put();

    sched_yield();
    debug_panic("Returned from process_exit");
}

void sched_thread_exit(void)
{
    Scheduler* scheduler = &smp_self()->scheduler;
    scheduler->runningThread->state = THREAD_STATE_KILLED;
    smp_put();

    sched_yield();
    debug_panic("Returned from thread_exit");
}

uint64_t sched_spawn(const char* path)
{
    Process* process = process_new(path);
    Thread* thread = thread_new(process, loader_entry, THREAD_PRIORITY_MIN);
    sched_push(thread, 1);

    return process->id;
}

//Temporary
uint64_t sched_local_thread_amount(void)
{
    Scheduler const* scheduler = &smp_self()->scheduler;

    uint64_t length = (scheduler->runningThread != NULL);
    for (uint64_t p = THREAD_PRIORITY_MIN; p <= THREAD_PRIORITY_MAX; p++)
    {
        length += queue_length(scheduler->queues[p]);
    }

    smp_put();
    return length;
}