#include <kernel/cpu/cpu.h>
#include <kernel/fs/namespace.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/mdl.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/io/verb.h>

#include <kernel/cpu/percpu.h>

typedef struct irp_ctx
{
    list_t timeouts;
    lock_t lock;
} irp_ctx_t;

PERCPU_DEFINE_CTOR(irp_ctx_t, pcpu_irps)
{
    irp_ctx_t* ctx = SELF_PTR(pcpu_irps);

    list_init(&ctx->timeouts);
    lock_init(&ctx->lock);
}

irp_pool_t* irp_pool_new(size_t size, process_t* process, void* ctx)
{
    if (size == 0 || process == NULL || size >= POOL_IDX_MAX)
    {
        errno = EINVAL;
        return NULL;
    }

    irp_pool_t* pool = malloc(sizeof(irp_pool_t) + (sizeof(irp_t) * size));
    if (pool == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    pool->ctx = ctx;
    pool->process = process;
    for (pool_idx_t i = 0; i < (pool_idx_t)size; i++)
    {
        irp_t* irp = &pool->irps[i];
        list_entry_init(&irp->entry);
        list_entry_init(&irp->timeoutEntry);
        atomic_init(&irp->cancel, NULL);
        irp->verb = VERB_MAX;
        irp->flags = 0;
        irp->timeout = CLOCKS_NEVER;
        irp->data = NULL;
        irp->arg0 = 0;
        irp->arg1 = 0;
        irp->arg2 = 0;
        irp->arg3 = 0;
        irp->arg4 = 0;
        irp->res._raw = 0;
        mdl_init(&irp->mdl, NULL);
        irp->sqe = (sqe_t){0};
        irp->index = i;
        irp->err = EOK;
        irp->cpu = CPU_ID_INVALID;
        irp->location = IRP_LOC_MAX;
        for (size_t j = 0; j < IRP_LOC_MAX; j++)
        {
            irp->stack[j].ctx = NULL;
            irp->stack[j].complete = NULL;
        }
    }

    pool_init(&pool->pool, pool->irps, size, sizeof(irp_t), offsetof(irp_t, next));

    return pool;
}

void irp_pool_free(irp_pool_t* pool)
{
    free(pool);
}

irp_t* irp_new(irp_pool_t* pool, sqe_t* sqe)
{
    pool_idx_t idx = pool_alloc(&pool->pool);
    if (idx == POOL_IDX_MAX)
    {
        errno = ENOSPC;
        return NULL;
    }

    irp_t* irp = &pool->irps[idx];
    irp->location = IRP_LOC_MAX;
    irp->next = POOL_IDX_MAX;
    irp->err = EINPROGRESS;
    irp->res._raw = 0;
    atomic_store_explicit(&irp->cancel, NULL, memory_order_relaxed);

    if (sqe == NULL)
    {
        irp->sqe = (sqe_t){0};
        irp->flags |= SQE_KERNEL;
    }
    else
    {
        irp->sqe = *sqe;
        irp->sqe.flags &= ~SQE_KERNEL;
    }

    irp->next = POOL_IDX_MAX;
    irp->cpu = CPU_ID_INVALID;
    for (size_t j = 0; j < IRP_LOC_MAX; j++)
    {
        irp->stack[j].ctx = NULL;
        irp->stack[j].complete = NULL;
    }

    if (atomic_load(&pool->pool.used) == 1)
    {
        REF(pool->process);
    }

    return irp;
}


void irp_free(irp_t* irp)
{
    if (irp == NULL)
    {
        return;
    }

    irp_timeout_remove(irp);

    assert(irp->location == IRP_LOC_MAX);
    assert(irp->next == POOL_IDX_MAX);
    assert(irp->cpu == CPU_ID_INVALID);

    verb_args_cleanup(irp);

    mdl_t* next = irp->mdl.next;
    mdl_deinit(&irp->mdl);
    mdl_free_chain(next, free);

    irp_pool_t* pool = irp_get_pool(irp);
    pool_free(&pool->pool, irp->index);

    if (atomic_load(&pool->pool.used) == 0)
    {
        UNREF(pool->process);
    }
}

uint64_t irp_cancel(irp_t* irp)
{
    irp_cancel_t handler = atomic_exchange(&irp->cancel, IRP_CANCELLED);
    if (handler == IRP_CANCELLED)
    {
        errno = EBUSY;
        return ERR;
    }

    if (handler == NULL)
    {
        errno = EBUSY;
        return ERR;
    }

    irp_timeout_remove(irp);

    irp->err = ECANCELED;
    return handler(irp);
}

void irp_timeout_add(irp_t* irp)
{
    if (irp->timeout == CLOCKS_NEVER)
    {
        return;
    }

    irp_ctx_t* ctx = SELF_PTR(pcpu_irps);
    LOCK_SCOPE(&ctx->lock);

    irp->cpu = SELF->id;

    clock_t now = clock_uptime();
    irp->deadline = CLOCKS_DEADLINE(irp->timeout, now);

    irp_t* entry;
    LIST_FOR_EACH(entry, &ctx->timeouts, timeoutEntry)
    {
        if (irp->deadline < entry->deadline)
        {
            list_prepend(&entry->timeoutEntry, &irp->timeoutEntry);
            timer_set(now, irp->deadline);
            return;
        }
    }

    list_push_back(&ctx->timeouts, &irp->timeoutEntry);
    timer_set(now, irp->deadline);
}

void irp_timeout_remove(irp_t* irp)
{
    cpu_id_t cpu = irp->cpu;
    if (cpu == CPU_ID_INVALID)
    {
        return;
    }

    irp_ctx_t* ctx = CPU_PTR(cpu, pcpu_irps);
    assert(ctx != NULL);

    LOCK_SCOPE(&ctx->lock);
    if (irp->cpu != cpu)
    {
        return;
    }

    list_remove(&irp->timeoutEntry);
    irp->cpu = CPU_ID_INVALID;
}

void irp_timeouts_check(void)
{
    irp_ctx_t* ctx = SELF_PTR(pcpu_irps);
    assert(ctx != NULL);

    clock_t now = clock_uptime();

    lock_acquire(&ctx->lock);

    irp_t* irp;
    while (true)
    {
        irp = CONTAINER_OF_SAFE(list_first(&ctx->timeouts), irp_t, timeoutEntry);
        if (irp == NULL)
        {
            break;
        }

        if (irp->deadline > now)
        {
            timer_set(now, irp->deadline);
            break;
        }

        list_remove(&irp->timeoutEntry);
        irp->deadline = CLOCKS_NEVER;
        irp->cpu = CPU_ID_INVALID;
        irp_cancel_t handler = atomic_exchange(&irp->cancel, IRP_CANCELLED);
        lock_release(&ctx->lock);

        if (handler == IRP_CANCELLED)
        {
            // Already cancelled
        }
        else if (handler != NULL)
        {
            irp->err = ETIMEDOUT;
            handler(irp);
        }

        lock_acquire(&ctx->lock);
    }

    lock_release(&ctx->lock);
}