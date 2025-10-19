#include "irq.h"

#include "cpu/smp.h"
#include "drivers/apic.h"
#include "interrupt.h"
#include "log/log.h"
#include "log/panic.h"
#include "sync/rwlock.h"

static rwlock_t lock = RWLOCK_CREATE;
static irq_handler_t handlers[IRQ_AMOUNT] = {0};

void irq_dispatch(interrupt_frame_t* frame)
{
    RWLOCK_READ_SCOPE(&lock);

    uint64_t irq = frame->vector - EXTERNAL_INTERRUPT_BASE;
    irq_handler_t* handler = &handlers[irq];

    bool handled = false;
    for (uint64_t i = 0; i < handler->callbackAmount; i++)
    {
        irq_callback_t* callback = &handler->callbacks[i];
        if (callback->func != NULL)
        {
            callback->func(irq, callback->data);
            handled = true;
        }
    }

    if (!handled)
    {
        LOG_WARN("unhandled irq %llu (vector=0x%x)\n", irq, frame->vector);
    }

    lapic_eoi();
}

void irq_install(irq_t irq, irq_callback_func_t func, void* data)
{
    RWLOCK_WRITE_SCOPE(&lock);

    irq_handler_t* handler = &handlers[irq];
    if (handler->callbackAmount >= IRQ_MAX_CALLBACK)
    {
        panic(NULL, "IRQ handler limit exceeded for irq=%d\n", irq);
    }

    uint64_t slot = handler->callbackAmount;
    handler->callbacks[slot].func = func;
    handler->callbacks[slot].data = data;
    handler->callbackAmount++;

    if (!handler->redirected)
    {
        ioapic_set_redirect(EXTERNAL_INTERRUPT_BASE + irq, irq, IOAPIC_DELIVERY_NORMAL, IOAPIC_POLARITY_HIGH,
            IOAPIC_TRIGGER_EDGE, smp_self_unsafe(), true);
    }

    LOG_INFO("installed handler for irq=%d slot=%u\n", irq, slot);
}

void irq_uninstall(irq_t irq, irq_callback_func_t func)
{
    RWLOCK_WRITE_SCOPE(&lock);

    irq_handler_t* handler = &handlers[irq];
    for (uint32_t i = 0; i < IRQ_MAX_CALLBACK; i++)
    {
        irq_callback_t* callback = &handler->callbacks[i];
        if (callback->func == func)
        {
            callback->func = NULL;
            callback->data = NULL;
            handler->callbackAmount--;
            LOG_INFO("uninstalled handler for irq=%d slot=%u\n", irq, i);
        }
    }
}
