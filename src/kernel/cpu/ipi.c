#include <kernel/cpu/cpu.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>

#include <errno.h>
#include <string.h>

static ipi_chip_t* registeredChip = NULL;
static rwlock_t chipLock = RWLOCK_CREATE;

void ipi_cpu_ctx_init(ipi_cpu_ctx_t* ctx)
{
    memset(ctx->queue, 0, sizeof(ctx->queue));
    ctx->readIndex = 0;
    ctx->writeIndex = 0;
    lock_init(&ctx->lock);
}

static void ipi_acknowledge(irq_t* irq)
{
    (void)irq;

    cpu_t* cpu = cpu_get_unsafe();
    ipi_cpu_ctx_t* ctx = &cpu->ipi;

    rwlock_read_acquire(&chipLock);
    if (registeredChip != NULL && registeredChip->ack != NULL)
    {
        registeredChip->ack(irq->cpu);
    }
    rwlock_read_release(&chipLock);

    while (true)
    {
        LOCK_SCOPE(&ctx->lock);

        if (ctx->readIndex == ctx->writeIndex)
        {
            break;
        }

        ipi_t ipi = ctx->queue[ctx->readIndex];
        ctx->readIndex = (ctx->readIndex + 1) % IPI_QUEUE_SIZE;

        ipi_func_data_t ipiData = {
            .self = cpu,
            .private = ipi.private,
        };
        ipi.func(&ipiData);
    }
}

static void ipi_eoi(irq_t* irq)
{
    (void)irq;

    cpu_t* cpu = cpu_get_unsafe();
    rwlock_read_acquire(&chipLock);
    if (registeredChip != NULL && registeredChip->eoi != NULL)
    {
        registeredChip->eoi(cpu);
    }
    rwlock_read_release(&chipLock);
}

static irq_chip_t ipiIrqChip = {
    .name = "IPI IRQ Chip",
    .enable = NULL,
    .disable = NULL,
    .ack = ipi_acknowledge,
    .eoi = ipi_eoi,
};

void ipi_init(void)
{
    if (irq_chip_register(&ipiIrqChip, VECTOR_IPI, VECTOR_IPI + 1, NULL) == ERR)
    {
        panic(NULL, "Failed to register IPI IRQ chip");
    }
}

uint64_t ipi_chip_register(ipi_chip_t* chip)
{
    if (chip == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&chipLock);

    if (registeredChip != NULL)
    {
        errno = EBUSY;
        return ERR;
    }

    registeredChip = chip;
    LOG_INFO("registered IPI chip '%s'\n", chip->name);
    return 0;
}

void ipi_chip_unregister(ipi_chip_t* chip)
{
    if (chip == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&chipLock);

    if (registeredChip != chip)
    {
        return;
    }

    LOG_INFO("unregistered IPI chip '%s'\n", chip->name);
    registeredChip = NULL;
}

uint64_t ipi_chip_amount(void)
{
    RWLOCK_READ_SCOPE(&chipLock);
    return registeredChip != NULL ? 1 : 0;
}

static uint64_t ipi_push(cpu_t* cpu, ipi_func_t func, void* private)
{
    if (registeredChip == NULL)
    {
        errno = ENODEV;
        return ERR;
    }

    if (registeredChip->interrupt == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    ipi_cpu_ctx_t* ctx = &cpu->ipi;
    LOCK_SCOPE(&ctx->lock);

    uint64_t nextWriteIndex = (ctx->writeIndex + 1) % IPI_QUEUE_SIZE;
    if (nextWriteIndex == ctx->readIndex)
    {
        errno = EBUSY;
        return ERR;
    }

    ctx->queue[ctx->writeIndex].func = func;
    ctx->queue[ctx->writeIndex].private = private;
    ctx->writeIndex = nextWriteIndex;
    return 0;
}

uint64_t ipi_send(cpu_t* cpu, ipi_flags_t flags, ipi_func_t func, void* private)
{
    if (func == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_READ_SCOPE(&chipLock);

    switch (flags & IPI_OTHERS)
    {
    case IPI_SINGLE:
    {
        if (cpu == NULL)
        {
            errno = EINVAL;
            return ERR;
        }

        if (ipi_push(cpu, func, private) == ERR)
        {
            return ERR;
        }

        registeredChip->interrupt(cpu, VECTOR_IPI);
    }
    break;
    case IPI_BROADCAST:
    {
        cpu_t* cpu;
        CPU_FOR_EACH(cpu)
        {
            if (ipi_push(cpu, func, private) == ERR)
            {
                return ERR;
            }

            registeredChip->interrupt(cpu, VECTOR_IPI);
        }
    }
    break;
    case IPI_OTHERS:
    {
        if (cpu == NULL)
        {
            errno = EINVAL;
            return ERR;
        }

        cpu_t* iter;
        CPU_FOR_EACH(iter)
        {
            if (iter == cpu)
            {
                continue;
            }

            if (ipi_push(iter, func, private) == ERR)
            {
                return ERR;
            }

            registeredChip->interrupt(iter, VECTOR_IPI);
        }
    }
    break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

void ipi_wake_up(cpu_t* cpu, ipi_flags_t flags)
{
    RWLOCK_READ_SCOPE(&chipLock);

    if (registeredChip == NULL || registeredChip->interrupt == NULL)
    {
        return;
    }

    switch (flags & IPI_OTHERS)
    {
    case IPI_SINGLE:
    {
        if (cpu == NULL)
        {
            return;
        }

        registeredChip->interrupt(cpu, VECTOR_IPI);
    }
    break;
    case IPI_BROADCAST:
    {
        cpu_t* cpu;
        CPU_FOR_EACH(cpu)
        {
            registeredChip->interrupt(cpu, VECTOR_IPI);
        }
    }
    break;
    case IPI_OTHERS:
    {
        if (cpu == NULL)
        {
            return;
        }

        cpu_t* iter;
        CPU_FOR_EACH(iter)
        {
            if (iter == cpu)
            {
                continue;
            }

            registeredChip->interrupt(iter, VECTOR_IPI);
        }
    }
    break;
    default:
        return;
    }
}

void ipi_invoke(void)
{
    IRQ_INVOKE(VECTOR_IPI);
}