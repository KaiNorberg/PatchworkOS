#include "heap.h"

#include "../platform/platform.h"

#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

static _PlatformMutex_t mutex;

static _HeapHeader_t* firstBlock;

void _HeapBlockSplit(_HeapHeader_t* block, uint64_t size)
{
    _HeapHeader_t* newBlock = (_HeapHeader_t*)((uint64_t)block + sizeof(_HeapHeader_t) + size);
    newBlock->size = block->size - sizeof(_HeapHeader_t) - size;
    newBlock->next = block->next;
    newBlock->reserved = false;
    newBlock->magic = _HEAP_HEADER_MAGIC;

    block->size = size;
    block->next = newBlock;
}

_HeapHeader_t* _HeapFirstBlock(void)
{
    if (firstBlock == NULL)
    {
        firstBlock = _HeapBlockNew(PAGE_SIZE - sizeof(_HeapHeader_t));
    }

    return firstBlock;
}

_HeapHeader_t* _HeapBlockNew(uint64_t size)
{
    uint64_t pageAmount = SIZE_IN_PAGES(size + sizeof(_HeapHeader_t));

    _HeapHeader_t* newBlock = _PlatformPageAlloc(pageAmount);
    if (newBlock == NULL)
    {
        return NULL;
    }

    newBlock->size = pageAmount * PAGE_SIZE - sizeof(_HeapHeader_t);
    newBlock->next = NULL;
    newBlock->reserved = false;
    newBlock->magic = _HEAP_HEADER_MAGIC;

    return newBlock;
}

void _HeapInit(void)
{
    _PLATFORM_MUTEX_INIT(&mutex);
    firstBlock = NULL;
}

void _HeapAcquire(void)
{
    _PLATFORM_MUTEX_ACQUIRE(&mutex);
}

void _HeapRelease(void)
{
    _PLATFORM_MUTEX_RELEASE(&mutex);
}