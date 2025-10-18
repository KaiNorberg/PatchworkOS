#include "heap.h"

#include "platform/platform.h"

#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

static _platform_mutex_t mutex;

static _heap_header_t* firstBlock;

static fd_t zeroResource;

static void* _heap_page_alloc(uint64_t amount)
{
    return mmap(zeroResource, NULL, amount * PAGE_SIZE, PROT_READ | PROT_WRITE);
}

void _heap_block_split(_heap_header_t* block, uint64_t size)
{
    _heap_header_t* newBlock = (_heap_header_t*)((uint64_t)block + sizeof(_heap_header_t) + size);
    newBlock->size = block->size - sizeof(_heap_header_t) - size;
    newBlock->next = block->next;
    newBlock->reserved = false;
    newBlock->magic = _HEAP_HEADER_MAGIC;

    block->size = size;
    block->next = newBlock;
}

_heap_header_t* _heap_block_new(uint64_t size)
{
    uint64_t pageAmount = BYTES_TO_PAGES(size + sizeof(_heap_header_t));

    _heap_header_t* newBlock = _heap_page_alloc(pageAmount);
    if (newBlock == NULL)
    {
        return NULL;
    }

    newBlock->size = pageAmount * PAGE_SIZE - sizeof(_heap_header_t);
    newBlock->next = NULL;
    newBlock->reserved = false;
    newBlock->magic = _HEAP_HEADER_MAGIC;

    return newBlock;
}

void _heap_init(void)
{
    _PLATFORM_MUTEX_INIT(&mutex);
    firstBlock = NULL;

    zeroResource = open("/dev/zero");
    if (zeroResource == ERR)
    {
        exit(EXIT_FAILURE);
    }
}

_heap_header_t* _heap_first_block(void)
{
    if (firstBlock == NULL)
    {
        firstBlock = _heap_block_new(PAGE_SIZE - sizeof(_heap_header_t));
    }

    return firstBlock;
}

void* _heap_alloc(uint64_t size)
{
    if (size == 0)
    {
        return NULL;
    }
    size = ROUND_UP(size, _HEAP_ALIGNMENT);

    _heap_header_t* currentBlock = _heap_first_block();
    if (currentBlock == NULL)
    {
        return NULL;
    }

    while (true)
    {
        if (!currentBlock->reserved)
        {
            if (currentBlock->size == size)
            {
                currentBlock->reserved = true;

                return _HEAP_HEADER_GET_START(currentBlock);
            }
            else if (currentBlock->size > size + sizeof(_heap_header_t) + _HEAP_ALIGNMENT)
            {
                currentBlock->reserved = true;
                _heap_block_split(currentBlock, size);

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

    _heap_header_t* newBlock = _heap_block_new(size);
    if (newBlock == NULL)
    {
        return NULL;
    }

    if (newBlock->size > size + sizeof(_heap_header_t) + _HEAP_ALIGNMENT)
    {
        _heap_block_split(newBlock, size);
    }
    currentBlock->next = newBlock;
    newBlock->reserved = true;

    return _HEAP_HEADER_GET_START(newBlock);
}

void _heap_free(void* ptr)
{
    _heap_header_t* block = (_heap_header_t*)((uint64_t)ptr - sizeof(_heap_header_t));
    block->reserved = false;
}

void _heap_acquire(void)
{
    _PLATFORM_MUTEX_ACQUIRE(&mutex);
}

void _heap_release(void)
{
    _PLATFORM_MUTEX_RELEASE(&mutex);
}
