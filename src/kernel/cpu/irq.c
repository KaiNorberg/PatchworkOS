#include "irq.h"

#include "log/log.h"
#include "pic.h"
#include "vectors.h"

static irq_handler_t handlers[IRQ_AMOUNT][IRQ_MAX_HANDLER];

void irq_dispatch(trap_frame_t* trapFrame)
{
    uint64_t irq = trapFrame->vector - VECTOR_IRQ_BASE;

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
            pic_clear_mask(irq);
            return;
        }
    }

    log_panic(NULL, "IRQ handler limit exceeded");
}
