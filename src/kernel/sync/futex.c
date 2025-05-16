#include "futex.h"
#include "utils/hashmap.h"
#include "lock.h"
#include "sched/sched.h"
#include "drivers/systime/systime.h"
#include "proc/thread.h"
#include "sched/wait.h"

#include <stdlib.h>

void futex_ctx_init(futex_ctx_t* ctx)
{
    hashmap_init(&ctx->futexes);
    lock_init(&ctx->lock);
}

void futex_ctx_deinit(futex_ctx_t* ctx)
{
    for (uint64_t i = 0; i < ctx->futexes.capacity; i++)
    {
        hashmap_entry_t* entry = ctx->futexes.entries[i];
        if (entry == NULL)
        {
            continue;
        }

        futex_t* futex = HASHMAP_CONTAINER(entry, futex_t, entry);
        wait_queue_deinit(&futex->queue);
        free(futex);
    }
    hashmap_deinit(&ctx->futexes);
}

static futex_t* futex_ctx_futex(futex_ctx_t* ctx, atomic_uint64* addr)
{
    LOCK_DEFER(&ctx->lock);

    futex_t* futex = HASHMAP_CONTAINER(hashmap_get(&ctx->futexes, (uint64_t)addr), futex_t, entry);
    if (futex != NULL)
    {
        return futex;
    }

    futex = malloc(sizeof(futex_t));
    hashmap_entry_init(&futex->entry);
    wait_queue_init(&futex->queue);

    hashmap_insert(&ctx->futexes, (uint64_t)addr, &futex->entry);
    return futex;
}

uint64_t futex_do(atomic_uint64* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    futex_t* futex = futex_ctx_futex(&sched_process()->futexCtx, addr);

    switch (op)
    {
    case FUTEX_WAIT:
    {
        if (atomic_load(addr) != val)
        {
            return ERROR(EAGAIN);
        }

        clock_t start = systime_uptime();
        wait_result_t result = WAIT_BLOCK_TIMEOUT(&futex->queue, atomic_load(addr) != val, timeout);
        if (result == WAIT_TIMEOUT)
        {
            return (systime_uptime() - start);
        }
    }
    break;
    case FUTEX_WAKE:
    {
        wait_unblock(&futex->queue, val);
    }
    break;
    case FUTEX_TRYLOCK:
    {
        if (atomic_compare_exchange_strong(addr, &(uint64_t){FUTEX_LOCKED}, FUTEX_UNLOCKED))
        {
            return FUTEX_LOCKED;
        }
        return FUTEX_UNLOCKED;
    }
    break;
    default:
    {
        return ERROR(EINVAL);
    }
    }

    return 0;
}
