#ifndef __EMBED__

#include "thrd.h"

static thrd_block_t blocks[_MAX_THRD];

void _ThrdInit(void)
{
    atomic_init(&blocks[0].ref, 1);
    atomic_init(&blocks[0].running, true);
    blocks[0].index = 0;
    blocks[0].id = gettid();
    blocks[0].result = 0;
    blocks[0].err = 0;
}

thrd_block_t* _ThrdBlockReserve(void)
{
    for (uint64_t i = 0; i < _MAX_THRD; i++)
    {
        atomic_long expected = 0;
        if (atomic_compare_exchange_strong(&blocks[i].ref, &expected, 1))
        {
            atomic_init(&blocks[i].running, false);
            blocks[i].index = i;
            blocks[i].id = 0;
            blocks[i].result = 0;
            blocks[i].err = 0;
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
