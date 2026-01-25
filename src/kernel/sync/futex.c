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
#include <kernel/utils/map.h>

#include <errno.h>
#include <stdlib.h>

void futex_ctx_init(futex_ctx_t* ctx)
{
    map_init(&ctx->futexes);
    lock_init(&ctx->lock);
}

void futex_ctx_deinit(futex_ctx_t* ctx)
{
    for (uint64_t i = 0; i < ctx->futexes.capacity; i++)
    {
        map_entry_t* entry = ctx->futexes.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }

        futex_t* futex = CONTAINER_OF(entry, futex_t, entry);
        wait_queue_deinit(&futex->queue);
        free(futex);
    }
    map_deinit(&ctx->futexes);
}

static futex_t* futex_ctx_get(futex_ctx_t* ctx, void* addr)
{
    LOCK_SCOPE(&ctx->lock);

    map_key_t key = map_key_uint64((uint64_t)addr);
    futex_t* futex = CONTAINER_OF_SAFE(map_get(&ctx->futexes, &key), futex_t, entry);
    if (futex != NULL)
    {
        return futex;
    }

    futex = malloc(sizeof(futex_t));
    if (futex == NULL)
    {
        return NULL;
    }
    map_entry_init(&futex->entry);
    wait_queue_init(&futex->queue);

    map_insert(&ctx->futexes, &key, &futex->entry);
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
        if (IS_FAIL(status))
        {
            return status;
        }

        uint64_t loadedVal;
        status = thread_load_atomic_from_user(thread, addr, &loadedVal);
        if (IS_FAIL(status))
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
        if (IS_FAIL(status))
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
