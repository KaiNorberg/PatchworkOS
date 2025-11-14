#include <kernel/cpu/cpu.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>

#include <errno.h>
#include <string.h>

static irq_handler_t* ipiHandler = NULL;

static ipi_chip_t* registeredChip = NULL;
static rwlock_t chipLock = RWLOCK_CREATE;

void ipi_cpu_ctx_init(ipi_cpu_ctx_t* ctx)
{
    memset(ctx->queue, 0, sizeof(ctx->queue));
    ctx->readIndex = 0;
    ctx->writeIndex = 0;
    lock_init(&ctx->lock);
}

static void ipi_handler_func(irq_func_data_t* data)
{
    ipi_cpu_ctx_t* ctx = &data->self->ipi;
    while (true)
    {
        LOCK_SCOPE(&ctx->lock);

        if (ctx->readIndex == ctx->writeIndex)
        {
            break;
        }

        ipi_t ipi = ctx->queue[ctx->readIndex];
        ctx->readIndex = (ctx->readIndex + 1) % IPI_QUEUE_SIZE;

        ipi_func_data_t ipiData = *data;
        ipiData.private = ipi.private;
        ipi.func(&ipiData);
    }
}

void ipi_init(void)
{
    ipiHandler = irq_handler_register(IRQ_VIRT_IPI, ipi_handler_func, NULL);
    if (ipiHandler == NULL)
    {
        panic(NULL, "failed to register IPI IRQ handler");
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
    LOG_INFO("registered IPI chip '%s'", chip->name);
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

    LOG_INFO("unregistered IPI chip '%s'", chip->name);
    registeredChip = NULL;
}

static uint64_t ipi_push(cpu_t* cpu, ipi_func_t func, void* private)
{
    if (registeredChip == NULL)
    {
        errno = ENODEV;
        return ERR;
    }

    if (registeredChip->invoke == NULL)
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

        registeredChip->invoke(cpu, IRQ_VIRT_IPI);
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

            registeredChip->invoke(cpu, IRQ_VIRT_IPI);
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

        cpu_t* sender = cpu_get_unsafe();
        cpu_t* cpu;
        CPU_FOR_EACH(cpu)
        {
            if (cpu == sender)
            {
                continue;
            }

            if (ipi_push(cpu, func, private) == ERR)
            {
                return ERR;
            }

            registeredChip->invoke(cpu, IRQ_VIRT_IPI);
        }
    }
    break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

void ipi_invoke(cpu_t* cpu, irq_virt_t virt)
{
    if (cpu == NULL)
    {
        return;
    }

    RWLOCK_READ_SCOPE(&chipLock);

    if (registeredChip == NULL || registeredChip->invoke == NULL)
    {
        return;
    }

    registeredChip->invoke(cpu, virt);
}