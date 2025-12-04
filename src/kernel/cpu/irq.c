#include <kernel/cpu/irq.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/rwlock.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/bitmap.h>
#include <sys/list.h>
#include <sys/math.h>

static irq_t irqs[VECTOR_EXTERNAL_AMOUNT] = {0};

// TODO: Optimize domain lookup?
static list_t domains = LIST_CREATE(domains);
static rwlock_t domainsLock = RWLOCK_CREATE();

static irq_t* irq_get(irq_virt_t virt)
{
    if (virt < VECTOR_EXTERNAL_START || virt >= VECTOR_EXTERNAL_END)
    {
        return NULL;
    }

    return &irqs[virt - VECTOR_EXTERNAL_START];
}

static uint64_t irq_update(irq_t* irq)
{
    if (irq->domain == NULL || irq->domain->chip == NULL)
    {
        return 0; // Nothing to do
    }

    if (!list_is_empty(&irq->handlers))
    {
        return irq->domain->chip->enable(irq);
    }

    irq->domain->chip->disable(irq);
    return 0;
}

// Must be called with domainsLock held
static irq_domain_t* irq_domain_lookup(irq_phys_t phys)
{
    irq_domain_t* domain;
    LIST_FOR_EACH(domain, &domains, entry)
    {
        if (phys >= domain->start && phys < domain->end)
        {
            return domain;
        }
    }

    return NULL;
}

static uint64_t irq_domain_rebind_orphaned_irqs(irq_domain_t* newDomain)
{
    for (irq_virt_t virt = VECTOR_EXTERNAL_START; virt < VECTOR_EXTERNAL_END; virt++)
    {
        irq_t* irq = irq_get(virt);
        assert(irq != NULL);

        rwlock_write_acquire(&irq->lock);
        if (irq->refCount == 0 || irq->phys < newDomain->start || irq->phys >= newDomain->end)
        {
            rwlock_write_release(&irq->lock);
            continue;
        }

        irq->domain = newDomain;
        if (irq_update(irq) == ERR)
        {
            irq->domain = NULL;
            rwlock_write_release(&irq->lock);

            for (irq_virt_t revVirt = VECTOR_EXTERNAL_START; revVirt < virt; revVirt++)
            {
                irq_t* revIrq = irq_get(revVirt);
                assert(revIrq != NULL);
                RWLOCK_WRITE_SCOPE(&revIrq->lock);

                if (revIrq == NULL || revIrq->domain != newDomain)
                {
                    continue;
                }

                if (!list_is_empty(&revIrq->handlers))
                {
                    newDomain->chip->disable(revIrq);
                }
                revIrq->domain = NULL;
            }
            return ERR;
        }

        LOG_INFO("mapped IRQ 0x%02x to 0x%02x while adding '%s'\n", irq->phys, virt, irq->domain->chip->name);
        rwlock_write_release(&irq->lock);
    }

    return 0;
}

void irq_init(void)
{
    for (uint64_t i = 0; i < VECTOR_EXTERNAL_AMOUNT; i++)
    {
        irq_t* irq = &irqs[i];
        irq->phys = IRQ_PHYS_NONE;
        irq->virt = VECTOR_EXTERNAL_START + i;
        irq->flags = 0;
        irq->cpu = NULL;
        irq->domain = NULL;
        irq->refCount = 0;
        list_init(&irq->handlers);
        rwlock_init(&irq->lock);
    }
}

void irq_dispatch(interrupt_frame_t* frame, cpu_t* self)
{
    assert(frame != NULL);
    assert(self != NULL);

    irq_t* irq = irq_get(frame->vector);
    if (irq == NULL)
    {
        panic(NULL, "Unexpected vector 0x%x dispatched through IRQ system", frame->vector);
    }

    RWLOCK_READ_SCOPE(&irq->lock);

    if (irq->domain == NULL || irq->domain->chip == NULL)
    {
        LOG_WARN("unhandled IRQ 0x%x received with no domain\n", frame->vector);
        return;
    }
    irq_chip_t* chip = irq->domain->chip;

    if (chip->ack != NULL)
    {
        chip->ack(irq);
    }

    irq_handler_t* handler;
    LIST_FOR_EACH(handler, &irq->handlers, entry)
    {
        irq_func_data_t data = {
            .frame = frame,
            .self = self,
            .virt = frame->vector,
            .private = handler->private,
        };

        handler->func(&data);
    }

    if (chip->eoi != NULL)
    {
        chip->eoi(irq);
    }
}

uint64_t irq_virt_alloc(irq_virt_t* out, irq_phys_t phys, irq_flags_t flags, cpu_t* cpu)
{
    if (out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_READ_SCOPE(&domainsLock);

    if (cpu == NULL)
    {
        cpu = cpu_get_unsafe();
    }

    irq_virt_t targetVirt = 0;
    for (irq_virt_t virt = VECTOR_EXTERNAL_START; virt < VECTOR_EXTERNAL_END; virt++)
    {
        irq_t* irq = irq_get(virt);
        assert(irq != NULL);
        rwlock_write_acquire(&irq->lock);

        if (irq->refCount > 0 && irq->phys == phys)
        {
            if (irq->flags != flags || irq->flags & IRQ_EXCLUSIVE)
            {
                rwlock_write_release(&irq->lock);
                errno = EBUSY;
                return ERR;
            }

            targetVirt = virt;
            break; // Lock still held
        }
        rwlock_write_release(&irq->lock);
    }

    if (targetVirt == 0)
    {
        for (irq_virt_t virt = VECTOR_EXTERNAL_START; virt < VECTOR_EXTERNAL_END; virt++)
        {
            irq_t* irq = irq_get(virt);
            assert(irq != NULL);
            rwlock_write_acquire(&irq->lock);

            if (irq->refCount == 0)
            {
                targetVirt = virt;
                break; // Lock still held
            }
            rwlock_write_release(&irq->lock);
        }
    }

    if (targetVirt == 0)
    {
        errno = ENOSPC;
        return ERR;
    }

    irq_t* irq = irq_get(targetVirt);
    assert(irq != NULL);

    irq->phys = phys;
    irq->virt = targetVirt;
    irq->flags = flags;
    irq->cpu = cpu;
    irq->domain = irq_domain_lookup(phys);

    if (irq_update(irq) == ERR)
    {
        rwlock_write_release(&irq->lock);
        return ERR;
    }

    irq->refCount++;
    rwlock_write_release(&irq->lock);

    *out = targetVirt;
    return 0;
}

void irq_virt_free(irq_virt_t virt)
{
    irq_t* irq = irq_get(virt);
    if (irq == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&irq->lock);

    if (irq->refCount == 0)
    {
        return;
    }

    irq->refCount--;
    if (irq->refCount > 0)
    {
        return;
    }

    if (!list_is_empty(&irq->handlers))
    {
        irq->domain->chip->disable(irq);
    }
    irq->phys = IRQ_PHYS_NONE;
    irq->flags = 0;
    irq->cpu = NULL;
    irq->domain = NULL;

    while (!list_is_empty(&irq->handlers))
    {
        irq_handler_t* handler = CONTAINER_OF(list_first(&irq->handlers), irq_handler_t, entry);
        list_remove(&irq->handlers, &handler->entry);
        free(handler);
    }
}

uint64_t irq_virt_set_affinity(irq_virt_t virt, cpu_t* cpu)
{
    if (cpu == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    irq_t* irq = irq_get(virt);
    if (irq == NULL)
    {
        errno = ENOENT;
        return ERR;
    }
    RWLOCK_WRITE_SCOPE(&irq->lock);

    if (irq->domain == NULL || irq->domain->chip == NULL)
    {
        errno = ENODEV;
        return ERR;
    }

    if (!list_is_empty(&irq->handlers))
    {
        irq->domain->chip->disable(irq);
        irq->cpu = cpu;
        if (irq->domain->chip->enable(irq) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        irq->cpu = cpu;
    }

    return 0;
}

uint64_t irq_chip_register(irq_chip_t* chip, irq_phys_t start, irq_phys_t end, void* private)
{
    if (chip == NULL || chip->enable == NULL || chip->disable == NULL || start >= end)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&domainsLock);

    irq_domain_t* existing;
    LIST_FOR_EACH(existing, &domains, entry)
    {
        if (MAX(start, existing->start) < MIN(end, existing->end))
        {
            errno = EEXIST;
            return ERR;
        }
    }

    irq_domain_t* domain = malloc(sizeof(irq_domain_t));
    if (domain == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }
    list_entry_init(&domain->entry);
    domain->chip = chip;
    domain->private = private;
    domain->start = start;
    domain->end = end;

    list_push_back(&domains, &domain->entry);

    if (irq_domain_rebind_orphaned_irqs(domain) == ERR)
    {
        list_remove(&domains, &domain->entry);
        free(domain);
        return ERR;
    }

    return 0;
}

void irq_chip_unregister(irq_chip_t* chip, irq_phys_t start, irq_phys_t end)
{
    if (chip == NULL || start >= end)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&domainsLock);

    irq_domain_t* domain;
    irq_domain_t* temp;
    LIST_FOR_EACH_SAFE(domain, temp, &domains, entry)
    {
        if (domain->chip != chip)
        {
            continue;
        }

        if (MAX(start, domain->start) >= MIN(end, domain->end))
        {
            continue;
        }

        for (irq_virt_t virt = VECTOR_EXTERNAL_START; virt < VECTOR_EXTERNAL_END; virt++)
        {
            irq_t* irq = irq_get(virt);
            assert(irq != NULL);

            rwlock_write_acquire(&irq->lock);
            if (irq->domain == domain)
            {
                if (!list_is_empty(&irq->handlers))
                {
                    irq->domain->chip->disable(irq);
                }
                irq->domain = NULL;
            }
            rwlock_write_release(&irq->lock);
        }

        list_remove(&domains, &domain->entry);
        free(domain);
    }
}

uint64_t irq_chip_amount(void)
{
    RWLOCK_READ_SCOPE(&domainsLock);
    return list_length(&domains);
}

uint64_t irq_handler_register(irq_virt_t virt, irq_func_t func, void* private)
{
    if (func == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    irq_t* irq = irq_get(virt);
    if (irq == NULL)
    {
        errno = ENOENT;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&irq->lock);

    irq_handler_t* iter;
    LIST_FOR_EACH(iter, &irq->handlers, entry)
    {
        if (iter->func == func)
        {
            errno = EEXIST;
            return ERR;
        }
    }

    irq_handler_t* handler = malloc(sizeof(irq_handler_t));
    if (handler == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }
    list_entry_init(&handler->entry);
    handler->func = func;
    handler->private = private;
    handler->virt = virt;

    list_push_back(&irq->handlers, &handler->entry);

    if (irq_update(irq) == ERR)
    {
        free(handler);
        return ERR;
    }

    return 0;
}

void irq_handler_unregister(irq_func_t func, irq_virt_t virt)
{
    if (func == NULL)
    {
        return;
    }

    irq_t* irq = irq_get(virt);
    if (irq == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&irq->lock);

    irq_handler_t* iter;
    LIST_FOR_EACH(iter, &irq->handlers, entry)
    {
        if (iter->func == func)
        {
            list_remove(&irq->handlers, &iter->entry);
            free(iter);

            if (list_is_empty(&irq->handlers))
            {
                irq->domain->chip->disable(irq);
            }
            return;
        }
    }

    LOG_WARN("attempted to unregister non-registered irq handler %p for irq 0x%x", func, virt);
}