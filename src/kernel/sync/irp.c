#include <_internal/clock_t.h>
#include <kernel/cpu/cpu.h>
#include <kernel/log/log.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/irp.h>
#include <kernel/sync/lock.h>

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

irp_pool_t* irp_pool_new(size_t size, void* ctx)
{
    if (size == 0 || size >= POOL_IDX_MAX)
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
        for (size_t j = 0; j < IRP_ARGS_MAX; j++)
        {
            irp->sqe._args[j] = 0;
        }
        irp->result = 0;
        irp->err = EOK;
        irp->index = i;
        irp->next = i < size - 1 ? i + 1 : POOL_IDX_MAX;
        irp->location = IRP_LOC_MAX;
        irp->cpu = CPU_ID_INVALID;
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

irp_t* irp_new(irp_pool_t* pool)
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
    irp->result = 0;
    irp->sqe = (sqe_t){0};
    atomic_store_explicit(&irp->cancel, NULL, memory_order_relaxed);
    irp->next = POOL_IDX_MAX;
    irp->cpu = CPU_ID_INVALID;
    for (size_t j = 0; j < IRP_LOC_MAX; j++)
    {
        irp->stack[j].ctx = NULL;
        irp->stack[j].complete = NULL;
    }
    return irp;
}

void irp_free(irp_t* irp)
{
    irp_pool_t* pool = irp_pool_get(irp);
    pool_free(&pool->pool, irp->index);
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
        lock_release(&ctx->lock);

        irp_cancel_t handler = atomic_exchange(&irp->cancel, IRP_CANCELLED);
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

void irp_dispatch(irp_t* irp)
{
    if (irp->err != EINPROGRESS)
    {
        irp_complete(irp);
        return;
    }

    if (irp->verb >= VERB_MAX || _irp_table_start[irp->verb].handler == NULL)
    {
        irp->err = ENOSYS;
        irp_complete(irp);
        return;
    }

    _irp_table_start[irp->verb].handler(irp);
}

static int irp_handler_cmp(const void* a, const void* b)
{
    const irp_handler_t* irpA = (const irp_handler_t*)a;
    const irp_handler_t* irpB = (const irp_handler_t*)b;
    return (int32_t)irpA->verb - (int32_t)irpB->verb;
}

void irp_table_init(void)
{
    const uint64_t irpsInTable = (((uint64_t)_irp_table_end - (uint64_t)_irp_table_start) / sizeof(irp_handler_t));
    assert(irpsInTable == VERB_MAX);

    LOG_INFO("sorting IRP table, total IRPs %d\n", VERB_MAX);
    qsort(_irp_table_start, irpsInTable, sizeof(irp_handler_t), irp_handler_cmp);

    for (uint64_t i = 0; i < irpsInTable; i++)
    {
        assert(_irp_table_start[i].verb == i);
    }
}

static uint64_t _nop_cancel(irp_t* irp)
{
    irp_complete(irp);
    return 0;
}

void nop_do(irp_t* irp)
{
    irp_set_cancel(irp, _nop_cancel);

    if (irp->timeout != CLOCKS_NEVER)
    {
        irp_timeout_add(irp);
        return;
    }
}

IRP_REGISTER(VERB_NOP, nop_do);