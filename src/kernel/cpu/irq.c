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

static irq_t irqs[IRQ_VIRT_TOTAL_AMOUNT] = {0};

// TODO: Optimize domain lookup?
static list_t domains = LIST_CREATE(domains);
static rwlock_t domainsLock = RWLOCK_CREATE;

void irq_init(void)
{
    for (irq_virt_t i = 0; i < IRQ_VIRT_TOTAL_AMOUNT; i++)
    {
        irq_t* irq = &irqs[i];
        irq->phys = UINT32_MAX;
        irq->virt = i;
        irq->flags = 0;
        irq->cpu = NULL;
        irq->domain = NULL;
        irq->refCount = 0;
        list_init(&irq->handlers);
        rwlock_init(&irq->lock);
    }

    for (irq_phys_t i = 0; i < IRQ_VIRT_EXTERNAL_START; i++)
    {
        irq_t* irq = &irqs[i];
        irq->refCount = UINT64_MAX; // Mark as used
    }
}

void irq_dispatch(interrupt_frame_t* frame, cpu_t* self)
{
    irq_virt_t virt = (irq_virt_t)frame->vector;

    if (virt >= IRQ_VIRT_TOTAL_AMOUNT)
    {
        panic(NULL, "invalid irq vector 0x%x", frame->vector);
    }

    if (virt < IRQ_VIRT_EXCEPTION_START)
    {
        panic(NULL, "exception irq 0x%x dispatched through irq system", virt);
    }

    irq_t* irq = &irqs[virt];
    RWLOCK_READ_SCOPE(&irq->lock);

    irq_handler_t* handler;
    LIST_FOR_EACH(handler, &irq->handlers, entry)
    {
        irq_func_data_t data = {
            .frame = frame,
            .self = self,
            .virt = virt,
            .private = handler->private,
        };

        handler->func(&data);
    }

    if (list_is_empty(&irq->handlers))
    {
        if (frame->vector < IRQ_VIRT_EXCEPTION_END)
        {
            if (INTERRUPT_FRAME_IN_USER_SPACE(frame))
            {
                LOG_WARN("unhandled user irq 0x%x received, killing process\n", virt);
                process_t* process = sched_thread_unsafe()->process;
                process_kill(process, EXIT_FAILURE);
                sched_invoke(frame, self, SCHED_DIE);
            }
            else
            {
                panic(frame, "unhandled kernel irq 0x%x received", virt);
            }
        }
        else
        {
            LOG_WARN("unhandled irq 0x%x received\n", virt);
        }
    }

    if (irq->domain == NULL || irq->domain->chip == NULL)
    {
        return;
    }

    irq_chip_t* chip = irq->domain->chip;
    if (chip->ack != NULL)
    {
        chip->ack(irq);
    }

    if (chip->eoi != NULL)
    {
        chip->eoi(irq);
    }
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

// Will return irq locked for write if successful
static irq_t* irq_find(irq_virt_t* outVirt, irq_phys_t phys)
{
    for (irq_virt_t virt = IRQ_VIRT_EXTERNAL_START; virt < IRQ_VIRT_TOTAL_AMOUNT; virt++)
    {
        irq_t* irq = &irqs[virt];
        rwlock_write_acquire(&irq->lock);

        if (irq->refCount != 0 && irq->phys == phys)
        {
            *outVirt = virt;
            return irq;
        }

        rwlock_write_release(&irq->lock);
    }

    return NULL;
}

// Will return irq locked for write if successful
static irq_t* irq_find_unused(irq_virt_t* outVirt)
{
    for (irq_virt_t virt = IRQ_VIRT_EXTERNAL_START; virt < IRQ_VIRT_TOTAL_AMOUNT; virt++)
    {
        irq_t* irq = &irqs[virt];
        rwlock_write_acquire(&irq->lock);

        if (irq->refCount == 0)
        {
            *outVirt = virt;
            return irq;
        }

        rwlock_write_release(&irq->lock);
    }

    return NULL;
}

uint64_t irq_virt_alloc(irq_virt_t* out, irq_phys_t phys, irq_flags_t flags, cpu_t* cpu)
{
    if (cpu == NULL)
    {
        cpu = cpu_get_unsafe();
    }

    RWLOCK_READ_SCOPE(&domainsLock);

    irq_virt_t virt;
    irq_t* irq = irq_find(&virt, phys);
    if (irq != NULL)
    {
        if (irq->flags != flags)
        {
            rwlock_write_release(&irq->lock);
            errno = EINVAL;
            return ERR;
        }

        if (irq->flags & IRQ_EXCLUSIVE)
        {
            rwlock_write_release(&irq->lock);
            errno = EBUSY;
            return ERR;
        }
    }
    else
    {
        irq = irq_find_unused(&virt);
        if (irq == NULL)
        {
            errno = ENOSPC;
            return ERR;
        }
    }

    irq->phys = phys;
    irq->virt = virt;
    irq->flags = flags;
    irq->cpu = cpu;
    irq->domain = irq_domain_lookup(phys); // Might be NULL, which is fine

    if (irq->domain != NULL && irq->domain->chip != NULL && irq->domain->chip->enable != NULL)
    {
        if (irq->domain->chip->enable(irq) == ERR)
        {
            rwlock_write_release(&irq->lock);
            return ERR;
        }

        LOG_INFO("mapped phys irq %d to virt irq 0x%x using '%s'\n", phys, virt, irq->domain->chip->name);
    }

    irq->refCount++;
    rwlock_write_release(&irq->lock);
    *out = virt;
    return 0;
}

void irq_virt_free(irq_virt_t virt)
{
    if (virt < IRQ_VIRT_EXTERNAL_START || virt >= IRQ_VIRT_TOTAL_AMOUNT)
    {
        return;
    }

    irq_t* irq = &irqs[virt];
    RWLOCK_WRITE_SCOPE(&irq->lock);

    if (irq->refCount == 0)
    {
        return;
    }

    if (--irq->refCount > 0)
    {
        return;
    }

    if (irq->domain != NULL && irq->domain->chip != NULL && irq->domain->chip->disable != NULL)
    {
        irq->domain->chip->disable(irq);
    }

    while (!list_is_empty(&irq->handlers))
    {
        irq_handler_t* handler = CONTAINER_OF(list_first(&irq->handlers), irq_handler_t, entry);
        list_remove(&irq->handlers, &handler->entry);
        free(handler);
    }
}

uint64_t irq_virt_set_affinity(irq_virt_t virt, cpu_t* cpu)
{
    if (virt < 0 || virt >= IRQ_VIRT_TOTAL_AMOUNT || cpu == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    irq_t* irq = &irqs[virt];
    RWLOCK_WRITE_SCOPE(&irq->lock);

    irq->domain->chip->disable(irq);
    irq->cpu = cpu;
    if (irq->domain->chip->enable(irq) == ERR)
    {
        return ERR;
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
            errno = EBUSY;
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

    for (irq_virt_t virt = IRQ_VIRT_EXTERNAL_START; virt < IRQ_VIRT_TOTAL_AMOUNT; virt++)
    {
        irq_t* irq = &irqs[virt];
        rwlock_write_acquire(&irq->lock);

        if (irq == NULL || irq->phys < start || irq->phys >= end)
        {
            rwlock_write_release(&irq->lock);
            continue;
        }

        irq->domain = domain;
        if (domain->chip->enable(irq) == ERR)
        {
            irq->domain = NULL;
            rwlock_write_release(&irq->lock);

            for (irq_virt_t revVirt = IRQ_VIRT_EXTERNAL_START; revVirt < virt; revVirt++)
            {
                irq_t* revIrq = &irqs[revVirt];
                RWLOCK_WRITE_SCOPE(&revIrq->lock);

                if (revIrq == NULL || revIrq->domain != domain)
                {
                    continue;
                }

                domain->chip->disable(revIrq);
                revIrq->domain = NULL;
            }
            list_remove(&domains, &domain->entry);
            free(domain);
            return ERR;
        }

        LOG_INFO("mapped phys irq %d to virt irq 0x%x while adding '%s'\n", irq->phys, virt, irq->domain->chip->name);
        rwlock_write_release(&irq->lock);
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

        for (irq_virt_t virt = IRQ_VIRT_EXTERNAL_START; virt < IRQ_VIRT_TOTAL_AMOUNT; virt++)
        {
            irq_t* irq = &irqs[virt];
            rwlock_write_acquire(&irq->lock);

            if (irq != NULL && irq->domain == domain)
            {
                domain->chip->disable(irq);
                LOG_INFO("disabled IRQ %u (phys 0x%lx) while removing chip %s", irq->virt, irq->phys,
                    chip->name);
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
    if (virt >= IRQ_VIRT_TOTAL_AMOUNT || func == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    irq_t* irq = &irqs[virt];
    RWLOCK_WRITE_SCOPE(&irq->lock);

    if (irq == NULL)
    {
        errno = ENOENT;
        return ERR;
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
    return 0;
}

void irq_handler_unregister(irq_handler_t* handler, irq_virt_t virt)
{
    if (handler == NULL || virt >= IRQ_VIRT_TOTAL_AMOUNT)
    {
        return;
    }

    irq_t* irq = &irqs[virt];
    RWLOCK_WRITE_SCOPE(&irq->lock);

    // Safety check
    irq_handler_t* iter;
    LIST_FOR_EACH(iter, &irq->handlers, entry)
    {
        if (iter == handler)
        {
            list_remove(&irq->handlers, &handler->entry);
            free(handler);
            return;
        }
    }

    LOG_WARN("attempted to unregister non-registered irq handler %p for irq 0x%x", handler, virt);
}
