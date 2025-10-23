#include "futex.h"
#include "cpu/syscalls.h"
#include "lock.h"
#include "mem/heap.h"
#include "proc/process.h"
#include "sched/sched.h"
#include "sched/timer.h"
#include "sched/wait.h"
#include "utils/map.h"

#include <errno.h>

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
        heap_free(futex);
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

    futex = heap_alloc(sizeof(futex_t), HEAP_NONE);
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
    process_t* process = sched_process();
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
            // Must pin before we start setting up the block becouse pining might also block.
            if (space_pin(space, addr, sizeof(atomic_uint64_t)) == ERR)
            {
                return ERR;
            }

            uptime = timer_uptime();
            clock_t remaining = (deadline == CLOCKS_NEVER) ? CLOCKS_NEVER : deadline - uptime;
            wait_queue_t* queue = &futex->queue;
            if (wait_block_setup(&queue, 1, remaining) == ERR)
            {
                space_unpin(space, addr, sizeof(atomic_uint64_t));
                return ERR;
            }

            bool condition = atomic_load(addr) != val;
            space_unpin(space, addr, sizeof(atomic_uint64_t));

            if (condition)
            {
                wait_block_cancel();
                if (firstCheck)
                {
                    errno = EAGAIN;
                }
                return 0;
            }
            firstCheck = false;

            // If a FUTEX_WAKE was called in between the check and the wait_block_commit() then we will unblock
            // immediately.
            if (wait_block_commit() == ERR)
            {
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
