#include <kernel/cpu/syscall.h>
#include <kernel/log/log.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/futex.h>
#include <kernel/sync/lock.h>

#include <stdlib.h>
#include <sys/map.h>

static bool futex_ctx_cmp(map_entry_t* entry, const void* key)
{
    futex_t* futex = CONTAINER_OF(entry, futex_t, entry);
    return futex->addr == (uintptr_t)key;
}

void futex_ctx_init(futex_ctx_t* ctx)
{
    MAP_DEFINE_INIT(ctx->futexes, futex_ctx_cmp);
    lock_init(&ctx->lock);
}

void futex_ctx_deinit(futex_ctx_t* ctx)
{
    LOCK_SCOPE(&ctx->lock);

    futex_t* futex;
    futex_t* temp;
    MAP_FOR_EACH_SAFE(futex, temp, &ctx->futexes, entry)
    {
        wait_queue_deinit(&futex->queue);
        free(futex);
    }
}

static futex_t* futex_ctx_get(futex_ctx_t* ctx, void* addr)
{
    LOCK_SCOPE(&ctx->lock);

    uint64_t hash = hash_uint64((uintptr_t)addr);
    futex_t* futex = CONTAINER_OF_SAFE(map_find(&ctx->futexes, addr, hash), futex_t, entry);
    if (futex == NULL)
    {
        return NULL;
    }

    futex = malloc(sizeof(futex_t));
    if (futex == NULL)
    {
        return NULL;
    }
    map_entry_init(&futex->entry);
    wait_queue_init(&futex->queue);

    map_insert(&ctx->futexes, &futex->entry, hash);
    return futex;
}

SYSCALL_DEFINE(SYS_FUTEX, atomic_uint64_t* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;
    futex_ctx_t* ctx = &process->futexCtx;

    futex_t* futex = futex_ctx_get(ctx, addr);
    if (futex == NULL)
    {
        return ERR(SYNC, NOMEM);
    }

    switch (op)
    {
    case FUTEX_WAIT:
    {
        wait_queue_t* queue = &futex->queue;

        status_t status = wait_block_prepare(&queue, 1, timeout);
        if (IS_ERR(status))
        {
            return status;
        }

        uint64_t loadedVal;
        status = thread_load_atomic_from_user(thread, addr, &loadedVal);
        if (IS_ERR(status))
        {
            wait_block_cancel();
            return status;
        }

        if (loadedVal != val)
        {
            wait_block_cancel();
            return ERR(SYNC, CHANGED);
        }

        status = wait_block_commit();
        if (IS_ERR(status))
        {
            return status;
        }

        return OK;
    }
    case FUTEX_WAKE:
    {
        return wait_unblock(&futex->queue, val, OK);
    }
    default:
    {
        return ERR(SYNC, INVAL);
    }
    }
}
