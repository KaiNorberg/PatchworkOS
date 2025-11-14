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

static irq_t internIrqs[IRQ_VIRT_EXTERNAL_START] = {0};

static irq_desc_t descriptors[IRQ_VIRT_TOTAL_AMOUNT] = {0};

// TODO: Optimize domain lookup?
static list_t domains = LIST_CREATE(domains);
static rwlock_t domainsLock = RWLOCK_CREATE;

void irq_init(void)
{
    for (irq_virt_t i = 0; i < IRQ_VIRT_TOTAL_AMOUNT; i++)
    {
        irq_desc_t* desc = &descriptors[i];
        desc->irq = NULL;
        list_init(&desc->handlers);
        rwlock_init(&desc->lock);
    }

    for (irq_virt_t i = 0; i < IRQ_VIRT_EXTERNAL_START; i++)
    {
        irq_t* irq = &internIrqs[i];
        irq->phys = 0;
        irq->virt = i;
        irq->flags = 0;
        irq->cpu = NULL;
        irq->domain = NULL;

        irq_desc_t* desc = &descriptors[i];
        desc->irq = irq;
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

    irq_desc_t* desc = &descriptors[virt];
    RWLOCK_READ_SCOPE(&desc->lock);

    irq_handler_t* handler;
    LIST_FOR_EACH(handler, &desc->handlers, entry)
    {
        irq_func_data_t data = {
            .frame = frame,
            .self = self,
            .virt = virt,
            .private = handler->private,
        };

        handler->func(&data);
    }

    if (list_is_empty(&desc->handlers))
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

    if (desc->irq == NULL || desc->irq->domain == NULL || desc->irq->domain->chip == NULL)
    {
        return;
    }

    irq_chip_t* chip = desc->irq->domain->chip;
    if (chip->ack != NULL)
    {
        chip->ack(desc->irq);
    }

    if (chip->eoi != NULL)
    {
        chip->eoi(desc->irq);
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

// Will return desc locked for write if successful
static irq_desc_t* irq_desc_find_unused(irq_virt_t* outVirt)
{
    for (irq_virt_t virt = IRQ_VIRT_EXTERNAL_START; virt < IRQ_VIRT_TOTAL_AMOUNT; virt++)
    {
        irq_desc_t* desc = &descriptors[virt];
        rwlock_write_acquire(&desc->lock);

        if (desc->irq == NULL)
        {
            *outVirt = virt;
            return desc;
        }

        rwlock_write_release(&desc->lock);
    }

    return NULL;
}

irq_t* irq_alloc(irq_phys_t phys, irq_flags_t flags, cpu_t* cpu)
{
    if (cpu == NULL)
    {
        cpu = cpu_get_unsafe();
    }

    RWLOCK_READ_SCOPE(&domainsLock);

    irq_domain_t* domain = irq_domain_lookup(phys); // Might be NULL, which is fine

    irq_virt_t virt;
    irq_desc_t* desc = irq_desc_find_unused(&virt);
    if (desc == NULL)
    {
        errno = ENOSPC;
        return NULL;
    }

    irq_t* irq = malloc(sizeof(irq_t));
    if (irq == NULL)
    {
        rwlock_write_release(&desc->lock);
        errno = ENOMEM;
        return NULL;
    }
    irq->phys = phys;
    irq->virt = virt;
    irq->flags = flags;
    irq->cpu = cpu;
    irq->domain = domain;

    if (domain != NULL && domain->chip != NULL && domain->chip->enable != NULL)
    {
        if (domain->chip->enable(irq) == ERR)
        {
            rwlock_write_release(&desc->lock);
            free(irq);
            return NULL;
        }

        LOG_INFO("mapped phys irq 0x%x to virt irq 0x%x for irq chip %s", phys, virt, domain->chip->name);
    }

    desc->irq = irq;
    rwlock_write_release(&desc->lock);
    return irq;
}

void irq_free(irq_t* irq)
{
    if (irq == NULL)
    {
        return;
    }

    if (irq->virt < IRQ_VIRT_EXTERNAL_START || irq->virt >= IRQ_VIRT_TOTAL_AMOUNT)
    {
        return;
    }

    irq_desc_t* desc = &descriptors[irq->virt];
    assert(desc->irq == irq);

    RWLOCK_WRITE_SCOPE(&desc->lock);

    if (irq->domain != NULL && irq->domain->chip != NULL && irq->domain->chip->disable != NULL)
    {
        irq->domain->chip->disable(irq);
    }

    while (!list_is_empty(&desc->handlers))
    {
        irq_handler_t* handler = CONTAINER_OF(list_first(&desc->handlers), irq_handler_t, entry);
        list_remove(&desc->handlers, &handler->entry);
        free(handler);
    }

    desc->irq = NULL;
    free(irq);
}

uint64_t irq_set_affinity(irq_t* irq, cpu_t* cpu)
{
    if (irq == NULL || irq->domain == NULL || cpu == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (irq->virt < IRQ_VIRT_EXTERNAL_START || irq->virt >= IRQ_VIRT_TOTAL_AMOUNT)
    {
        errno = EINVAL;
        return ERR;
    }

    irq_desc_t* desc = &descriptors[irq->virt];
    RWLOCK_WRITE_SCOPE(&desc->lock);

    irq->domain->chip->disable(irq);
    irq->cpu = cpu;
    if (irq->domain->chip->enable(irq) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t irq_chip_register(irq_chip_t* chip, irq_phys_t start, irq_phys_t end)
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
    domain->start = start;
    domain->end = end;

    list_push_back(&domains, &domain->entry);

    for (irq_virt_t virt = IRQ_VIRT_EXTERNAL_START; virt < IRQ_VIRT_TOTAL_AMOUNT; virt++)
    {
        irq_desc_t* desc = &descriptors[virt];
        rwlock_write_acquire(&desc->lock);

        if (desc->irq == NULL || desc->irq->phys < start || desc->irq->phys >= end)
        {
            rwlock_write_release(&desc->lock);
            continue;
        }

        desc->irq->domain = domain;
        if (domain->chip->enable(desc->irq) == ERR)
        {
            desc->irq->domain = NULL;
            rwlock_write_release(&desc->lock);

            for (irq_virt_t revVirt = IRQ_VIRT_EXTERNAL_START; revVirt < virt; revVirt++)
            {
                irq_desc_t* revDesc = &descriptors[revVirt];
                RWLOCK_WRITE_SCOPE(&revDesc->lock);

                if (revDesc->irq == NULL || revDesc->irq->domain != domain)
                {
                    continue;
                }

                domain->chip->disable(revDesc->irq);
                revDesc->irq->domain = NULL;
            }
            list_remove(&domains, &domain->entry);
            free(domain);
            return ERR;
        }

        LOG_INFO("mapped phys irq 0x%x to virt irq 0x%x while adding irq chip %s", desc->irq->phys, virt, chip->name);
        rwlock_write_release(&desc->lock);
    }

    return 0;
}

void irq_chip_unregister(irq_chip_t* chip)
{
    if (chip == NULL)
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

        for (irq_virt_t virt = IRQ_VIRT_EXTERNAL_START; virt < IRQ_VIRT_TOTAL_AMOUNT; virt++)
        {
            irq_desc_t* desc = &descriptors[virt];
            rwlock_write_acquire(&desc->lock);

            if (desc->irq != NULL && desc->irq->domain == domain)
            {
                domain->chip->disable(desc->irq);
                LOG_INFO("disabled IRQ %u (phys 0x%lx) while removing chip %s", desc->irq->virt, desc->irq->phys,
                    chip->name);
                desc->irq->domain = NULL;
            }

            rwlock_write_release(&desc->lock);
        }

        list_remove(&domains, &domain->entry);
        free(domain);
    }
}

irq_handler_t* irq_handler_register(irq_virt_t virt, irq_func_t func, void* private)
{
    if (virt >= IRQ_VIRT_TOTAL_AMOUNT || func == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    irq_desc_t* desc = &descriptors[virt];
    RWLOCK_WRITE_SCOPE(&desc->lock);

    irq_handler_t* handler = malloc(sizeof(irq_handler_t));
    if (handler == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    list_entry_init(&handler->entry);
    handler->func = func;
    handler->private = private;
    handler->virt = virt;

    list_push_back(&desc->handlers, &handler->entry);
    return handler;
}

void irq_handler_unregister(irq_handler_t* handler)
{
    if (handler == NULL)
    {
        return;
    }

    irq_virt_t virt = handler->virt;
    if (virt >= IRQ_VIRT_TOTAL_AMOUNT)
    {
        return;
    }

    irq_desc_t* desc = &descriptors[virt];
    RWLOCK_WRITE_SCOPE(&desc->lock);

    // Safety check
    irq_handler_t* iter;
    LIST_FOR_EACH(iter, &desc->handlers, entry)
    {
        if (iter == handler)
        {
            list_remove(&desc->handlers, &handler->entry);
            free(handler);
            return;
        }
    }

    LOG_WARN("attempted to unregister non-registered irq handler %p for irq 0x%x", handler, virt);
}
