#include "dispatcher.h"

#include "queue/queue.h"
#include "apic/apic.h"
#include "master/master.h"
#include "master/interrupts/interrupts.h"

static Queue* readyQueue;
static Queue* waitQueues[IRQ_AMOUNT];

void dispatcher_init()
{        
    readyQueue = queue_new();

    for (uint64_t i = 0; i < IRQ_AMOUNT; i++)
    {
        waitQueues[i] = queue_new();
    }
}

Callback dispatcher_fetch()
{
    if (queue_length(readyQueue) != 0)
    {
        return queue_pop(readyQueue);
    }

    return 0;
}

void dispatcher_dispatch(uint8_t irq)
{    
    while (queue_length(waitQueues[irq]) != 0)
    {
        queue_push(readyQueue, queue_pop(waitQueues[irq]));
    }
}

void dispatcher_send(uint8_t irq)
{
    local_apic_send_ipi(master_local_apic_id(), IRQ_BASE + irq);
}

void dispatcher_push(Callback callback, uint8_t irq)
{    
    queue_push(waitQueues[irq], callback);
}