#include "heap.h"

#include "platform/platform.h"

#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

static _PlatformMutex_t mutex;

static _HeapHeader_t* firstBlock;

static fd_t zeroResource;

static void* _HeapPageAlloc(uint64_t amount)
{
    return mmap(zeroResource, NULL, amount * PAGE_SIZE, PROT_READ | PROT_WRITE);
}

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

_HeapHeader_t* _HeapBlockNew(uint64_t size)
{
    uint64_t pageAmount = BYTES_TO_PAGES(size + sizeof(_HeapHeader_t));

    _HeapHeader_t* newBlock = _HeapPageAlloc(pageAmount);
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

    zeroResource = open("sys:/zero");
    if (zeroResource == ERR)
    {
        exit(EXIT_FAILURE);
    }
}

_HeapHeader_t* _HeapFirstBlock(void)
{
    if (firstBlock == NULL)
    {
        firstBlock = _HeapBlockNew(PAGE_SIZE - sizeof(_HeapHeader_t));
    }

    return firstBlock;
}

void* _HeapAlloc(uint64_t size)
{
    if (size == 0)
    {
        return NULL;
    }
    size = ROUND_UP(size, _HEAP_ALIGNMENT);

    _HeapHeader_t* currentBlock = _HeapFirstBlock();
    while (true)
    {
        if (!currentBlock->reserved)
        {
            if (currentBlock->size == size)
            {
                currentBlock->reserved = true;

                return _HEAP_HEADER_GET_START(currentBlock);
            }
            else if (currentBlock->size > size + sizeof(_HeapHeader_t) + _HEAP_ALIGNMENT)
            {
                currentBlock->reserved = true;
                _HeapBlockSplit(currentBlock, size);

                return _HEAP_HEADER_GET_START(currentBlock);
            }
        }

        if (currentBlock->next != NULL)
        {
            currentBlock = currentBlock->next;
        }
        else
        {
            break;
        }
    }

    _HeapHeader_t* newBlock = _HeapBlockNew(size);
    if (newBlock == NULL)
    {
        return NULL;
    }

    if (newBlock->size > size + sizeof(_HeapHeader_t) + _HEAP_ALIGNMENT)
    {
        _HeapBlockSplit(newBlock, size);
    }
    currentBlock->next = newBlock;
    newBlock->reserved = true;

    return _HEAP_HEADER_GET_START(newBlock);
}

void _HeapFree(void* ptr)
{
    _HeapHeader_t* block = (_HeapHeader_t*)((uint64_t)ptr - sizeof(_HeapHeader_t));
    block->reserved = false;
}

void _HeapAcquire(void)
{
    _PLATFORM_MUTEX_ACQUIRE(&mutex);
}

void _HeapRelease(void)
{
    _PLATFORM_MUTEX_RELEASE(&mutex);
}
