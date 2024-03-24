#include "irq.h"

#include "apic/apic.h"
#include "pic/pic.h"
#include "debug/debug.h"

static IrqHandler handlers[IRQ_AMOUNT][IRQ_MAX_HANDLER_AMOUNT];

void irq_dispatch(InterruptFrame* interruptFrame)
{
    uint64_t irq = interruptFrame->vector - IRQ_BASE;

    for (uint64_t i = 0; i < IRQ_MAX_HANDLER_AMOUNT; i++)
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

    //TODO: Replace with io apic
    pic_eoi(irq);
}

void irq_install_handler(IrqHandler handler, uint8_t irq)
{
    for (uint64_t i = 0; i < IRQ_MAX_HANDLER_AMOUNT; i++)
    {
        if (handlers[irq][i] == NULL)
        {
            handlers[irq][i] = handler;
            return;
        }
    }

    debug_panic("IRQ handler limit exceeded");
}