#include "scheduler.h"

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
#include "program_loader/program_loader.h"
#include "scheduler/schedule/schedule.h"

#include <libc/string.h>

static Scheduler** schedulers;

static void scheduler_allocate_schedulers(void)
{
    schedulers = kmalloc(sizeof(Scheduler*) * smp_cpu_amount());
    memset(schedulers, 0, sizeof(Scheduler*) * smp_cpu_amount());

    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        Scheduler* scheduler = kmalloc(sizeof(Scheduler));
        scheduler->id = i;
        for (uint64_t p = PROCESS_PRIORITY_MIN; p <= PROCESS_PRIORITY_MAX; p++)
        {
            scheduler->queues[p] = queue_new();
        }
        scheduler->runningProcess = 0;

        schedulers[i] = scheduler;
    }
}

static void scheduler_spawn_init_process(void)
{
    Process* process = process_new(0);
    process->timeEnd = UINT64_MAX;
    process->priority = PROCESS_PRIORITY_MAX;

    scheduler_local()->runningProcess = process;
    scheduler_put();
}

void scheduler_init(void)
{
    tty_start_message("Scheduler initializing");

    scheduler_allocate_schedulers();

    scheduler_spawn_init_process();

    smp_send_ipi_to_others(IPI_START);
    asm volatile("sti");
    SMP_SEND_IPI_TO_SELF(IPI_START);

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_cpu_start(void)
{
    apic_timer_init(IPI_BASE + IPI_SCHEDULE, SCHEDULER_TIMER_HZ);
}

Scheduler* scheduler_get(uint64_t id)
{
    return schedulers[id];
}

//Must have a corresponding call to scheduler_put()
Scheduler* scheduler_local(void)
{
    return schedulers[smp_self()->id];
}

void scheduler_put(void)
{
    smp_put();
}

Process* scheduler_process(void)
{
    Process* process = scheduler_local()->runningProcess;
    scheduler_put();

    return process;
}

void scheduler_exit(Status status)
{
    Scheduler* scheduler = scheduler_local();
    scheduler->runningProcess->state = PROCESS_STATE_KILLED;
    scheduler_put();
    
    scheduler_yield();
}

int64_t scheduler_spawn(const char* path)
{    
    Process* process = process_new(program_loader_entry);
    if (process == 0)
    {
        return -1;
    }

    //Temporary: For now the executable is passed via the user stack to the program loader.
    //Eventually it will be passed via a system similar to "/proc/self/exec".
    void* stackBottom = address_space_map(process->addressSpace, (void*)(VMM_LOWER_HALF_MAX - PAGE_SIZE), 1);
    void* stackTop = (void*)((uint64_t)stackBottom + PAGE_SIZE);
    uint64_t pathLength = strlen(path);
    void* dest = (void*)((uint64_t)stackTop - pathLength - 1);
    memcpy(dest, path, pathLength + 1);
    process->interruptFrame->stackPointer -= pathLength + 1;
    process->interruptFrame->rdi = VMM_LOWER_HALF_MAX - pathLength - 1;

    scheduler_push(process, 1, -1);

    return process->id;
}

//Temporary
uint64_t scheduler_local_process_amount(void)
{
    Scheduler const* scheduler = scheduler_local();
    
    uint64_t length = (scheduler->runningProcess != 0);
    for (int64_t priority = PROCESS_PRIORITY_MAX; priority >= PROCESS_PRIORITY_MIN; priority--) 
    {
        length += queue_length(scheduler->queues[priority]);
    }

    scheduler_put();

    return length;
}