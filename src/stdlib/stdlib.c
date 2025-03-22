#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

#include "common/heap.h"
#include "platform/platform.h"

#if _PLATFORM_HAS_SCHEDULING
_NORETURN void exit(int status)
{
    _PlatformExit(status);
}
#endif

static void* malloc_unlocked(size_t size)
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

static void free_unlocked(void* ptr)
{
    _HeapHeader_t* block = (_HeapHeader_t*)((uint64_t)ptr - sizeof(_HeapHeader_t));
    /*if (block->magic != _HEAP_HEADER_MAGIC)
    {
        _PlatformPanic("Invalid heap magic\n");
    }
    else if (!block->reserved)
    {
        _PlatformPanic("Attempt to free unreserved block at %a, size %d", ptr, block->size);
    }*/
    block->reserved = false;
}

void* malloc(size_t size)
{
    _HeapAcquire();
    void* ptr = malloc_unlocked(size);
    _HeapRelease();
    return ptr;
}

void* calloc(size_t num, size_t size)
{
    void* data = malloc(num * size);
    if (data == NULL)
    {
        return NULL;
    }
    memset(data, 0, num * size);
    return data;
}

void* realloc(void* ptr, size_t size)
{
    _HeapAcquire();
    _HeapHeader_t* block = (_HeapHeader_t*)((uint64_t)ptr - sizeof(_HeapHeader_t));

    /*if (block->magic != _HEAP_HEADER_MAGIC)
    {
        _PlatformPanic("Invalid heap magic\n");
    }*/

    void* newPtr = malloc_unlocked(size);
    memcpy(newPtr, ptr, MIN(size, block->size));
    free_unlocked(ptr);

    _HeapRelease();
    return newPtr;
}

void free(void* ptr)
{
    _HeapAcquire();
    free_unlocked(ptr);
    _HeapRelease();
}

char* lltoa(long long number, char* str, int base)
{
    char* p = str;
    long long i = number;

    if (i < 0)
    {
        *p++ = '-';
        i = -i;
    }

    uint64_t shifter = i;
    do
    {
        ++p;
        shifter = shifter / base;
    } while (shifter);

    *p = '\0';
    do
    {
        uint8_t digit = i % base;

        *--p = digit < 10 ? '0' + digit : 'A' + digit - 10;
        i = i / base;
    } while (i);

    return str;
}

char* ulltoa(unsigned long long number, char* str, int base)
{
    char* p = str;
    unsigned long long i = number;

    uint64_t shifter = i;
    do
    {
        ++p;
        shifter = shifter / base;
    } while (shifter);

    *p = '\0';
    do
    {
        uint8_t digit = i % base;

        *--p = digit < 10 ? '0' + digit : 'A' + digit - 10;
        i = i / base;
    } while (i);

    return str;
}
