#ifndef __EMBED__

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <sys/proc.h>
#include <threads.h>

#define _MAX_THRD 32

typedef struct
{
    atomic_long ref;
    atomic_bool running;
    uint8_t index;
    tid_t id;
    uint8_t result;
    int err;
} thrd_block_t;

void _ThrdInit(void);

thrd_block_t* _ThrdBlockReserve(void);

void _ThrdBlockFree(thrd_block_t* block);

uint64_t _ThrdIndexById(tid_t id);

thrd_block_t* _ThrdBlockById(tid_t id);

thrd_block_t* _ThrdBlockByIndex(uint64_t index);

static inline thrd_block_t* _ThrdBlockRef(thrd_block_t* block)
{
    atomic_fetch_add(&block->ref, 1);
    return block;
}

static inline void _ThrdBlockUnref(thrd_block_t* block)
{
    if (atomic_fetch_sub(&block->ref, 1) <= 1)
    {
        _ThrdBlockFree(block);
    }
}

#endif
