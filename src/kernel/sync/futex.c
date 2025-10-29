#include <kernel/cpu/syscalls.h>
#include <kernel/log/log.h>
#include <kernel/proc/process.h>
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
    futex_t* futex = CONTAINER_OF(map_get(&ctx->futexes, &key), futex_t, entry);
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

SYSCALL_DEFINE(SYS_FUTEX, uint64_t, atomic_uint64_t* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;
    space_t* space = &process->space;
    futex_ctx_t* ctx = &process->futexCtx;

    futex_t* futex = futex_ctx_get(ctx, addr);
    if (futex == NULL)
    {
        return ERR;
    }

    switch (op)
    {
    case FUTEX_WAIT:
    {
        clock_t uptime = timer_uptime();

        clock_t deadline;
        if (timeout == CLOCKS_NEVER)
        {
            deadline = CLOCKS_NEVER;
        }
        else if (timeout > CLOCKS_NEVER - uptime)
        {
            deadline = CLOCKS_NEVER;
        }
        else
        {
            deadline = uptime + timeout;
        }

        bool firstCheck = true;
        while (true)
        {
            uptime = timer_uptime();
            clock_t remaining = (deadline == CLOCKS_NEVER) ? CLOCKS_NEVER : deadline - uptime;
            wait_queue_t* queue = &futex->queue;
            if (wait_block_setup(&queue, 1, remaining) == ERR)
            {
                return ERR;
            }

            uint64_t loadedVal;
            if (thread_load_atomic_from_user(thread, addr, &loadedVal) == ERR)
            {
                wait_block_cancel();
                return ERR;
            }

            if (loadedVal != val)
            {
                if (firstCheck)
                {
                    errno = EAGAIN;
                }
                wait_block_cancel();
                return 0;
            }
            firstCheck = false;

            // If a FUTEX_WAKE was called in between the check and the wait_block_commit() then we will unblock
            // immediately.
            if (wait_block_commit() == ERR)
            {
                LOG_DEBUG("futex wait interrupted tid=%d\n", thread->id);
                return ERR;
            }
        }
    }
    break;
    case FUTEX_WAKE:
    {
        if (wait_unblock(&futex->queue, val, EOK) == ERR)
        {
            return ERR;
        }
    }
    break;
    default:
    {
        errno = EINVAL;
        return ERR;
    }
    }
    return 0;
}
