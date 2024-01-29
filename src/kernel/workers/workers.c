#include "workers.h"

#include "idt/idt.h"
#include "gdt/gdt.h"
#include "apic/apic.h"
#include "page_allocator/page_allocator.h"
#include "string/string.h"
#include "utils/utils.h"
#include "tty/tty.h"
#include "madt/madt.h"
#include "hpet/hpet.h"
#include "master/master.h"
#include "debug/debug.h"
#include "global_heap/global_heap.h"

#include "workers/interrupts/interrupts.h"
#include "workers/startup/startup.h"

static Worker workers[MAX_WORKER_AMOUNT];
static uint8_t workerAmount;

static Idt* idt;

void workers_init()
{
    tty_start_message("Workers initializing");

    idt = gmalloc(1);
    worker_idt_populate(idt);

    workers_startup(workers, &workerAmount);

    tty_end_message(TTY_MESSAGE_OK);
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
        Worker* worker = worker_get(i);

        if (worker->present && worker->apicId == apicId)
        {
            return worker;
        }
    }    

    debug_panic("Unable to find worker");
    return 0;
}