#include <kernel/cpu/cpu.h>
#include <kernel/fs/namespace.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/mdl.h>
#include <kernel/mem/pool.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <stdatomic.h>
#include <string.h>

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
    atomic_init(&pool->active, 0);
    memset(&pool->irps, 0, sizeof(irp_t) * size);
    for (size_t i = 0; i < size; i++)
    {
        irp_t* irp = &pool->irps[i];
        irp->index = i;
    }

    pool_init(&pool->pool, pool->irps, size, sizeof(irp_t), offsetof(irp_t, next));

    return pool;
}

void irp_pool_free(irp_pool_t* pool)
{
    free(pool);
}

void irp_timeout_add(irp_t* irp, clock_t timeout)
{
    if (timeout == CLOCKS_NEVER)
    {
        return;
    }

    irp_ctx_t* ctx = SELF_PTR(pcpu_irps);
    LOCK_SCOPE(&ctx->lock);

    irp->cpu = SELF->id;

    clock_t now = clock_uptime();
    irp->deadline = CLOCKS_DEADLINE(timeout, now);

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
    if (irp->cpu != cpu) // Check for race condition
    {
        return;
    }

    list_remove(&irp->timeoutEntry);
    irp->cpu = CPU_ID_INVALID;
}

static void irp_perform_completion(irp_t* irp)
{
    while (irp->frame < IRP_FRAME_MAX)
    {
        irp_frame_t* frame = irp_current(irp);
        irp->frame++;

        if (irp->frame == IRP_FRAME_MAX)
        {
            irp_timeout_remove(irp);
        }

        if (frame->vnode != NULL)
        {
            UNREF(frame->vnode);
            frame->vnode = NULL;
        }

        if (frame->complete != NULL)
        {
            frame->complete(irp, frame->ctx);
            return;
        }
    }

    assert(irp->frame == IRP_FRAME_MAX);
    assert(irp->next == POOL_IDX_MAX);
    assert(irp->cpu == CPU_ID_INVALID);

    mdl_t* next = irp->mdl.next;
    mdl_deinit(&irp->mdl);
    mdl_free_chain(next, free);

    irp_pool_t* pool = irp_get_pool(irp);
    pool_free(&pool->pool, irp->index);

    if (atomic_fetch_sub(&pool->active, 1) == 1)
    {
        UNREF(pool->process);
    }
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
            irp_perform_completion(irp);
        }
        else
        {
            atomic_store(&irp->cancel, NULL);
        }

        lock_acquire(&ctx->lock);
    }

    lock_release(&ctx->lock);
}

irp_t* irp_new(irp_pool_t* pool)
{
    pool_idx_t idx = pool_alloc(&pool->pool);
    if (idx == POOL_IDX_MAX)
    {
        errno = ENOSPC;
        return NULL;
    }

    if (atomic_fetch_add(&pool->active, 1) == 0)
    {
        REF(pool->process);
    }

    irp_t* irp = &pool->irps[idx];
    assert(irp->index == idx);

    list_entry_init(&irp->entry);
    list_entry_init(&irp->timeoutEntry);
    atomic_init(&irp->cancel, NULL);
    irp->deadline = CLOCKS_NEVER;
    irp->res._raw = 0;
    mdl_init(&irp->mdl, NULL);
    irp->next = POOL_IDX_MAX;
    irp->cpu = CPU_ID_INVALID;
    irp->err = EOK;
    irp->frame = IRP_FRAME_MAX;
    return irp;
}

mdl_t* irp_get_mdl(irp_t* irp, const void* addr, size_t size)
{
    if (irp == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    process_t* process = irp_get_process(irp);
    if (process == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    mdl_t* mdl = &irp->mdl;
    while (mdl->amount > 0)
    {
        if (mdl->next != NULL)
        {
            mdl = mdl->next;
            continue;
        }

        mdl_t* next = malloc(sizeof(mdl_t));
        if (next == NULL)
        {
            errno = ENOMEM;
            return NULL;
        }
        mdl_init(next, mdl);
        mdl = next;
    }

    if (mdl_add(mdl, &process->space, addr, size) == ERR)
    {
        return NULL;
    }

    return mdl;
}

void irp_call(irp_t* irp, vnode_t* vnode)
{
    assert(irp->frame > 0);
    irp->frame--;

    irp_frame_t* frame = irp_current(irp);
    if (UNLIKELY(frame->major >= IRP_MJ_MAX))
    {
        irp_error(irp, EINVAL);
        return;
    }

    if (vnode == NULL || vnode->vtable == NULL)
    {
        irp_error(irp, EINVAL);
        return;
    }

    irp_func_t func = vnode->vtable->funcs[frame->major];
    if (func == NULL)
    {
        irp_error(irp, ENOSYS);
        return;
    }

    atomic_store_explicit(&irp->cancel, NULL, memory_order_relaxed);
    frame->vnode = REF(vnode);

    func(irp);
}

void irp_call_direct(irp_t* irp, irp_func_t func)
{
    assert(irp->frame > 0);
    irp->frame--;

    irp_frame_t* frame = irp_current(irp);

    if (frame->vnode != NULL)
    {
        UNREF(frame->vnode);
        frame->vnode = NULL;
    }

    atomic_store_explicit(&irp->cancel, NULL, memory_order_relaxed);
    func(irp);
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
        atomic_store(&irp->cancel, NULL);
        errno = EBUSY;
        return ERR;
    }

    irp_timeout_remove(irp);

    irp->err = ECANCELED;
    uint64_t result = handler(irp);
    irp_perform_completion(irp);
    return result;
}

void irp_complete(irp_t* irp)
{
    if (irp_set_cancel(irp, NULL) == IRP_CANCELLED)
    {
        return;
    }
    irp_perform_completion(irp);
}