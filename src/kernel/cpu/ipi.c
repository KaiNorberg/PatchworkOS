#include <kernel/cpu/cpu.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/irq.h>
#include <kernel/cpu/percpu.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>

#include <string.h>
#include <sys/status.h>

static ipi_chip_t* registeredChip = NULL;
static rwlock_t chipLock = RWLOCK_CREATE();

PERCPU_DEFINE_CTOR(static void, pcpu_ipi)
{
    ipi_cpu_t* ctx = SELF_PTR(pcpu_ipi);

    memset(ctx->queue, 0, sizeof(ctx->queue));
    ctx->readIndex = 0;
    ctx->writeIndex = 0;
    lock_init(&ctx->lock);
}

void ipi_handle_pending(interrupt_frame_t* frame)
{
    UNUSED(frame);

    ipi_cpu_t* ctx = SELF_PTR(pcpu_ipi);

    rwlock_read_acquire(&chipLock);
    if (registeredChip != NULL && registeredChip->ack != NULL)
    {
        registeredChip->ack();
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
            .data = ipi.data,
        };
        ipi.func(&ipiData);
    }

    rwlock_read_acquire(&chipLock);
    if (registeredChip != NULL && registeredChip->eoi != NULL)
    {
        registeredChip->eoi();
    }
    rwlock_read_release(&chipLock);
}

status_t ipi_chip_register(ipi_chip_t* chip)
{
    if (chip == NULL)
    {
        return ERR(INT, INVAL);
    }

    RWLOCK_WRITE_SCOPE(&chipLock);

    if (registeredChip != NULL)
    {
        return ERR(INT, BUSY);
    }

    registeredChip = chip;
    LOG_INFO("registered IPI chip '%s'\n", chip->name);
    return OK;
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

static status_t ipi_push(cpu_t* cpu, ipi_func_t func, void* data)
{
    if (registeredChip == NULL)
    {
        return ERR(INT, NODEV);
    }

    if (registeredChip->interrupt == NULL)
    {
        return ERR(INT, IMPL);
    }

    ipi_cpu_t* ctx = CPU_PTR(cpu->id, pcpu_ipi);
    LOCK_SCOPE(&ctx->lock);

    size_t nextWriteIndex = (ctx->writeIndex + 1) % IPI_QUEUE_SIZE;
    if (nextWriteIndex == ctx->readIndex)
    {
        return ERR(INT, BUSY);
    }

    ctx->queue[ctx->writeIndex].func = func;
    ctx->queue[ctx->writeIndex].data = data;
    ctx->writeIndex = nextWriteIndex;
    return OK;
}

status_t ipi_send(cpu_t* cpu, ipi_flags_t flags, ipi_func_t func, void* data)
{
    if (func == NULL)
    {
        return ERR(INT, INVAL);
    }

    RWLOCK_READ_SCOPE(&chipLock);

    switch (flags & IPI_OTHERS)
    {
    case IPI_SINGLE:
    {
        if (cpu == NULL)
        {
            return ERR(INT, INVAL);
        }

        status_t status = ipi_push(cpu, func, data);
        if (IS_ERR(status))
        {
            return status;
        }

        registeredChip->interrupt(cpu, VECTOR_IPI);
    }
    break;
    case IPI_BROADCAST:
    {
        cpu_t* cpu;
        CPU_FOR_EACH(cpu)
        {
            status_t status = ipi_push(cpu, func, data);
            if (IS_ERR(status))
            {
                return status;
            }

            registeredChip->interrupt(cpu, VECTOR_IPI);
        }
    }
    break;
    case IPI_OTHERS:
    {
        if (cpu == NULL)
        {
            return ERR(INT, INVAL);
        }

        cpu_t* iter;
        CPU_FOR_EACH(iter)
        {
            if (iter == cpu)
            {
                continue;
            }

            status_t status = ipi_push(iter, func, data);
            if (IS_ERR(status))
            {
                return status;
            }

            registeredChip->interrupt(iter, VECTOR_IPI);
        }
    }
    break;
    default:
        return ERR(INT, INVAL);
    }

    return OK;
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