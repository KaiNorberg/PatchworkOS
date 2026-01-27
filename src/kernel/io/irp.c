#include <kernel/cpu/cpu.h>
#include <kernel/fs/namespace.h>
#include <kernel/io/irp.h>
#include <kernel/io/irp_cleanup.h>
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

status_t irp_pool_new(irp_pool_t** out, size_t size, process_t* process, void* ctx)
{
    if (out == NULL || size == 0 || process == NULL || size >= POOL_IDX_MAX)
    {
        return ERR(IO, INVAL);
    }

    irp_pool_t* pool = malloc(sizeof(irp_pool_t) + (sizeof(irp_t) * size));
    if (pool == NULL)
    {
        return ERR(IO, NOMEM);
    }

    pool->ctx = ctx;
    pool->process = process;
    pool->size = size;
    atomic_init(&pool->active, 0);
    memset(&pool->irps, 0, sizeof(irp_t) * size);
    for (size_t i = 0; i < size; i++)
    {
        irp_t* irp = &pool->irps[i];
        irp->index = i;
    }

    pool_init(&pool->pool, pool->irps, size, sizeof(irp_t), offsetof(irp_t, next));

    *out = pool;
    return OK;
}

void irp_pool_free(irp_pool_t* pool)
{
    free(pool);
}

void irp_pool_cancel_all(irp_pool_t* pool)
{
    if (pool == NULL)
    {
        return;
    }

    for (size_t i = 0; i < pool->size; i++)
    {
        irp_cancel(&pool->irps[i]);
    }
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

        irp_cleanup_args(frame);

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

    atomic_store(&irp->cancel, NULL);

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
            irp->status = INFO(IO, TIMEOUT);
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

status_t irp_get(irp_t** out, irp_pool_t* pool)
{
    pool_idx_t idx = pool_alloc(&pool->pool);
    if (idx == POOL_IDX_MAX)
    {
        return ERR(IO, NOSPACE);
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
    irp->status = OK;
    irp->frame = IRP_FRAME_MAX;
    
    *out = irp;
    return OK;
}

status_t irp_get_mdl(irp_t* irp, mdl_t** out, const void* addr, size_t size)
{
    if (irp == NULL || out == NULL || addr == NULL || size == 0)
    {
        return ERR(IO, INVAL);
    }

    process_t* process = irp_get_process(irp);
    assert(process != NULL);

    mdl_t* current = &irp->mdl;
    while (current->amount > 0)
    {
        if (current->next != NULL)
        {
            current = current->next;
            continue;
        }

        mdl_t* next = malloc(sizeof(mdl_t));
        if (next == NULL)
        {
            return ERR(IO, NOMEM);
        }
        mdl_init(next, current);
        current = next;
    }

    status_t status = mdl_from_region(current, NULL, &process->space, addr, size);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = current;
    return OK;
}

void irp_call(irp_t* irp, vnode_t* vnode)
{
    assert(irp->frame > 0);
    irp->frame--;

    irp_frame_t* frame = irp_current(irp);
    if (UNLIKELY(frame->major >= IRP_MJ_MAX))
    {
        irp_complete(irp, ERR(IO, MJ_OVERFLOW));
        return;
    }

    if (vnode == NULL || vnode->vtable == NULL)
    {
        irp_complete(irp, ERR(IO, MJ_NOSYS));
        return;
    }

    irp_func_t func = vnode->vtable->funcs[frame->major];
    if (func == NULL)
    {
        irp_complete(irp, ERR(IO, MJ_NOSYS));
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

status_t irp_cancel(irp_t* irp)
{
    irp_cancel_t handler = atomic_exchange(&irp->cancel, IRP_CANCELLED);
    if (handler == IRP_CANCELLED)
    {
        return ERR(IO, CANCELLED);
    }

    if (handler == NULL)
    {
        atomic_store(&irp->cancel, NULL);
        return ERR(IO, NOT_CANCELLABLE);
    }

    irp_timeout_remove(irp);

    irp->status = INFO(IO, CANCELLED);
    status_t status = handler(irp);
    irp_perform_completion(irp);
    return status;
}

void irp_complete(irp_t* irp, status_t status)
{
    if (irp_set_cancel(irp, NULL) == IRP_CANCELLED)
    {
        return;
    }
    if (status != OK)
    {
        irp->status = status;
    }
    irp_perform_completion(irp);
}