#include "scheduler.h"

#include <string.h>

#include "vmm/vmm.h"
#include "smp/smp.h"
#include "heap/heap.h"
#include "gdt/gdt.h"
#include "time/time.h"
#include "debug/debug.h"
#include "registers/registers.h"
#include "interrupts/interrupts.h"
#include "irq/irq.h"
#include "apic/apic.h"
#include "hpet/hpet.h"
#include "program_loader/program_loader.h"
#include "scheduler/schedule/schedule.h"

static uint8_t scheduler_sleep_callback(uint64_t deadline)
{
    return deadline <= time_nanoseconds();
}

static void scheduler_spawn_init_thread(void)
{
    Process* process = process_new();

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
    scheduler->runningThread = 0;
}

void scheduler_start(void)
{
    scheduler_spawn_init_thread();

    smp_send_ipi_to_others(IPI_START);
    asm volatile("sti");
    SMP_SEND_IPI_TO_SELF(IPI_START);
}

void scheduler_cpu_start(void)
{
    apic_timer_init(IPI_BASE + IPI_SCHEDULE, SCHEDULER_TIMER_HZ);
}

Thread* scheduler_thread(void)
{
    Thread* thread = smp_self()->scheduler.runningThread;
    smp_put();

    return thread;
}

Process* scheduler_process(void)
{
    Process* process = smp_self()->scheduler.runningThread->process;
    smp_put();

    return process;
}

void scheduler_yield(void)
{
    SMP_SEND_IPI_TO_SELF(IPI_SCHEDULE);
}

void scheduler_sleep(uint64_t nanoseconds)
{
    Blocker blocker =
    {
        .context = time_nanoseconds() + nanoseconds,
        .callback = scheduler_sleep_callback
    };
    scheduler_block(blocker);
}

void scheduler_block(Blocker blocker)
{
    if (blocker.callback(blocker.context))
    {
        return;
    }

    Scheduler* scheduler = &smp_self()->scheduler;
    Thread* thread = scheduler->runningThread;
    thread->blocker = blocker;
    thread->state = THREAD_STATE_BLOCKED;
    smp_put();

    scheduler_yield();
}

void scheduler_process_exit(uint64_t status)
{
    //TODO: Add handling for status

    Scheduler* scheduler = &smp_self()->scheduler;
    scheduler->runningThread->state = THREAD_STATE_KILLED;
    scheduler->runningThread->process->killed = 1;
    smp_put();

    scheduler_yield();
}

void scheduler_thread_exit(void)
{
    Scheduler* scheduler = &smp_self()->scheduler;
    scheduler->runningThread->state = THREAD_STATE_KILLED;
    smp_put();

    scheduler_yield();
}

uint64_t scheduler_spawn(const char* path)
{
    Process* process = process_new();
    Thread* thread = thread_new(process, program_loader_entry, THREAD_PRIORITY_MIN);

    //Temporary: For now the executable is passed via the user stack to the program loader.
    //Eventually it will be passed via a system similar to "/proc/self/exec".
    void* stackBottom = address_space_allocate(process->addressSpace, (void*)(VMM_LOWER_HALF_MAX - PAGE_SIZE), 1);
    void* stackTop = (void*)((uint64_t)stackBottom + PAGE_SIZE);
    uint64_t pathLength = strlen(path);
    void* dest = (void*)((uint64_t)stackTop - pathLength - 1);
    memcpy(dest, path, pathLength + 1);
    thread->interruptFrame.stackPointer -= pathLength + 1;
    thread->interruptFrame.rdi = VMM_LOWER_HALF_MAX - pathLength - 1;

    scheduler_push(thread, 1, -1);

    return process->id;
}

//Temporary
uint64_t scheduler_local_thread_amount(void)
{
    Scheduler const* scheduler = &smp_self()->scheduler;

    uint64_t length = (scheduler->runningThread != 0);
    for (int64_t p = THREAD_PRIORITY_MIN; p <= THREAD_PRIORITY_MAX; p++)
    {
        length += queue_length(scheduler->queues[p]);
    }

    smp_put();

    return length;
}