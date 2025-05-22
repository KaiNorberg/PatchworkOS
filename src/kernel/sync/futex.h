#pragma once

#include "fs/vfs.h"
#include "sched/wait.h"
#include "utils/hashmap.h"

#include <sys/proc.h>

// TODO: Implement futex support for shared memory.

typedef struct
{
    hashmap_entry_t entry;
    wait_queue_t queue;
} futex_t;

typedef struct
{
    hashmap_t futexes;
    lock_t lock;
} futex_ctx_t;

void futex_ctx_init(futex_ctx_t* ctx);

void futex_ctx_deinit(futex_ctx_t* ctx);

uint64_t futex_do(atomic_uint64* addr, uint64_t val, futex_op_t op, clock_t timeout);
