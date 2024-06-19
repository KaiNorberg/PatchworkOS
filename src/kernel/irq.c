#include "irq.h"

#include "debug.h"
#include "pic.h"

static irq_handler_t handlers[IRQ_AMOUNT][IRQ_MAX_HANDLER];

void irq_dispatch(trap_frame_t* trapFrame)
{
    uint64_t irq = trapFrame->vector - IRQ_BASE;

    for (uint64_t i = 0; i < IRQ_MAX_HANDLER; i++)
    {
        if (handlers[irq][i] != NULL)
        {
            handlers[irq][i](irq);
        }
        else
        {
            break;
        }
    }

    // TODO: Replace with io apic
    pic_eoi(irq);
}

void irq_install(irq_handler_t handler, uint8_t irq)
{
    for (uint64_t i = 0; i < IRQ_MAX_HANDLER; i++)
    {
        if (handlers[irq][i] == NULL)
        {
            handlers[irq][i] = handler;
            return;
        }
    }

    debug_panic("IRQ handler limit exceeded");
}
