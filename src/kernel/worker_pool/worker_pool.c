#include "worker_pool.h"

#include "idt/idt.h"
#include "gdt/gdt.h"
#include "apic/apic.h"
#include "page_allocator/page_allocator.h"
#include "utils/utils.h"
#include "tty/tty.h"
#include "madt/madt.h"
#include "hpet/hpet.h"
#include "master/master.h"
#include "debug/debug.h"
#include "global_heap/global_heap.h"

#include "worker/interrupts/interrupts.h"
#include "worker/scheduler/scheduler.h"
#include "worker/program_loader/program_loader.h"
#include "worker/startup/startup.h"

static Worker workers[MAX_WORKER_AMOUNT];
static uint8_t workerAmount;

static Idt* idt;

void worker_pool_init()
{
    tty_start_message("Workers initializing");

    idt = gmalloc(1);
    worker_idt_populate(idt);

    workers_startup(workers, &workerAmount);

    tty_end_message(TTY_MESSAGE_OK);
}

void worker_pool_send_ipi(Ipi ipi)
{
    for (uint8_t i = 0; i < workerAmount; i++)
    {
        worker_send_ipi(worker_get(i), ipi);
    }
}

//Temporary
void worker_pool_spawn(const char* path)
{        
    Process* process = process_new(PROCESS_PRIORITY_MIN);
    if (load_program(process, path) != STATUS_SUCCESS)
    {
        process_free(process);
        return;
    }

    for (uint8_t i = 0; i < workerAmount; i++)
    {
        scheduler_acquire(worker_get(i)->scheduler);
    }

    uint64_t bestLength = -1;
    Scheduler* bestScheduler = 0;
    for (uint8_t i = 0; i < workerAmount; i++)
    {
        Scheduler* scheduler = worker_get(i)->scheduler;
        uint64_t length = (scheduler->runningProcess != 0);
        for (int64_t priority = PROCESS_PRIORITY_MAX; priority >= PROCESS_PRIORITY_MIN; priority--) 
        {
            length += queue_length(scheduler->queues[priority]);
        }

        if (bestLength > length)
        {
            bestLength = length;
            bestScheduler = scheduler;
        }
    }

    scheduler_push(bestScheduler, process);

    for (uint8_t i = 0; i < workerAmount; i++)
    {
        scheduler_release(worker_get(i)->scheduler);
    }
}

uint8_t worker_amount()
{
    return workerAmount;
}

Idt* worker_idt_get()
{
    return idt;
}

Worker* worker_get(uint8_t id)
{
    return &workers[id];
}

Worker* worker_self()
{
    uint64_t id = read_msr(MSR_WORKER_ID);
    if (id >= MAX_WORKER_AMOUNT)
    {
        debug_panic("Invalid worker");
        return 0;
    }

    return &workers[id];
}

Worker* worker_self_brute()
{
    uint8_t apicId = local_apic_id();
    for (uint16_t i = 0; i < MAX_WORKER_AMOUNT; i++)
    {
        Worker* worker = worker_get((uint8_t)i);

        if (worker->present && worker->apicId == apicId)
        {
            return worker;
        }
    }    

    debug_panic("Unable to find worker");
    return 0;
}