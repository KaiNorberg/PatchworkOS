#ifndef __EMBED__

#include "thrd.h"

static thrd_block_t blocks[_MAX_THRD];

void _ThrdInit(void)
{
    atomic_init(&blocks[0].ref, 1);
    atomic_init(&blocks[0].running, true);
    blocks[0].index = 0;
    _ThrdBlockInit(&blocks[0], NULL, NULL, gettid());

    for (uint64_t i = 1; i < _MAX_THRD; i++)
    {
        atomic_init(&blocks[i].ref, 0);
        atomic_init(&blocks[i].running, false);
        blocks[i].index = i;
    }
}

void _ThrdBlockInit(thrd_block_t* block, int (*func)(void*), void* arg, tid_t id)
{
    block->id = id;
    block->result = 0;
    block->func = func;
    block->arg = arg;
    block->err = 0;
}

thrd_block_t* _ThrdBlockReserve(void)
{
    for (uint64_t i = 0; i < _MAX_THRD; i++)
    {
        atomic_long expected = 0;
        if (atomic_compare_exchange_strong(&blocks[i].ref, &expected, 1))
        {
            return &blocks[i];
        }
    }

    return NULL;
}

void _ThrdBlockFree(thrd_block_t* block)
{
    atomic_store(&block->running, false);
    atomic_store(&block->ref, 0);
}

thrd_block_t* _ThrdBlockById(tid_t id)
{
    for (uint64_t i = 0; i < _MAX_THRD; i++)
    {
        if (blocks[i].id == id && atomic_load(&blocks[i].ref) != 0)
        {
            return _ThrdBlockRef(&blocks[i]);
        }
    }

    return NULL;
}

thrd_block_t* _ThrdBlockByIndex(uint64_t index)
{
    if (index >= _MAX_THRD)
    {
        return NULL;
    }

    if (atomic_load(&blocks[index].ref) == 0)
    {
        return NULL;
    }

    return _ThrdBlockRef(&blocks[index]);
}

#endif
