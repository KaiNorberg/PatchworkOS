#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

#include "common/heap.h"

void* realloc(void* ptr, size_t size)
{
    if (ptr == NULL)
    {
        return malloc(size);
    }

    _HeapAcquire();
    _HeapHeader_t* block = (_HeapHeader_t*)((uint64_t)ptr - sizeof(_HeapHeader_t));
    if (block->size == ROUND_UP(size, _HEAP_ALIGNMENT))
    {
        _HeapRelease();
        return ptr;
    }

    void* newPtr = _HeapAlloc(size);
    if (newPtr == NULL)
    {
        _HeapRelease();
        return NULL;
    }
    memcpy(newPtr, ptr, MIN(size, block->size));
    _HeapFree(ptr);

    _HeapRelease();
    return newPtr;
}
