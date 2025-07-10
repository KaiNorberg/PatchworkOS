#include "futex.h"
#include "drivers/systime/systime.h"
#include "lock.h"
#include "mem/heap.h"
#include "sched/sched.h"
#include "sched/thread.h"
#include "sched/wait.h"
#include "utils/map.h"

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
        if (entry == NULL)
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
    map_key_t key = map_key_uint64((uint64_t)addr);
    futex_t* futex = CONTAINER_OF(map_get(&ctx->futexes, &key), futex_t, entry);
    if (futex != NULL)
    {
        return futex;
    }

    futex = heap_alloc(sizeof(futex_t), HEAP_NONE);
    map_entry_init(&futex->entry);
    wait_queue_init(&futex->queue);

    map_insert(&ctx->futexes, &key, &futex->entry);
    return futex;
}

uint64_t futex_do(atomic_uint64_t* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    futex_ctx_t* ctx = &sched_process()->futexCtx;
    LOCK_DEFER(&ctx->lock);

    futex_t* futex = futex_ctx_get(ctx, addr);

    switch (op)
    {
    case FUTEX_WAIT:
    {
        if (atomic_load(addr) != val)
        {
            errno = EAGAIN;
            return ERR;
        }

        clock_t start = systime_uptime();
        wait_result_t result = WAIT_BLOCK_LOCK_TIMEOUT(&futex->queue, &ctx->lock, atomic_load(addr) != val, timeout);
        if (result == WAIT_TIMEOUT)
        {
            return (systime_uptime() - start);
        }
    }
    break;
    case FUTEX_WAKE:
    {
        return wait_unblock(&futex->queue, val);
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

SYSCALL_DEFINE(SYS_FUTEX, uint64_t, atomic_uint64_t* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    return futex_do(addr, val, op, timeout);
}
