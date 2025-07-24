#include "irq.h"

#include "cpu/smp.h"
#include "log/log.h"
#include "log/panic.h"
#include "sync/rwlock.h"
#include "cpu/apic.h"
#include "vectors.h"

static rwlock_t lock = RWLOCK_CREATE();
static irq_handler_t handlers[IRQ_AMOUNT] = {0};

void irq_dispatch(trap_frame_t* trapFrame)
{
    RWLOCK_READ_SCOPE(&lock);

    uint64_t irq = trapFrame->vector - VECTOR_IRQ_BASE;

    bool handled = false;
    for (uint64_t i = 0; i < handlers[irq].callbackAmount; i++)
    {
        if (handlers[irq].callbacks[i] != NULL)
        {
            handlers[irq].callbacks[i](irq);
            handled = true;
        }
    }

    if (!handled)
    {
        LOG_WARN("unhandled irq %llu (vector=0x%x)\n", irq, trapFrame->vector);
    }

    lapic_eoi();
}

void irq_install(irq_callback_t callback, uint8_t irq)
{
    RWLOCK_WRITE_SCOPE(&lock);

    if (handlers[irq].callbackAmount >= IRQ_MAX_CALLBACK)
    {
        panic(NULL, "IRQ handler limit exceeded for irq=%d\n", irq);
    }

    handlers[irq].callbacks[handlers[irq].callbackAmount] = callback;
    handlers[irq].callbackAmount++;
    if (!handlers[irq].redirected)
    {
        ioapic_set_redirect(VECTOR_IRQ_BASE + irq, irq, IOAPIC_DELIVERY_NORMAL, IOAPIC_POLARITY_HIGH, IOAPIC_TRIGGER_EDGE, smp_self_unsafe(), true);
    }

    LOG_INFO("installed handler for irq=%d slot=%u\n", irq, handlers[irq].callbackAmount - 1);
}

void irq_uninstall(irq_callback_t callback, uint8_t irq)
{
    RWLOCK_WRITE_SCOPE(&lock);

    for (uint32_t i = 0; i < IRQ_MAX_CALLBACK; i++)
    {
        if (handlers[irq].callbacks[i] == callback)
        {
            handlers[irq].callbacks[i] = NULL;
            handlers[irq].callbackAmount--;
            LOG_INFO("uninstalled handler for irq=%d slot=%u\n", irq, i);
        }
    }
}
