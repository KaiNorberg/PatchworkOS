#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

#include "platform/user/common/heap.h"

void* realloc(void* ptr, size_t size)
{
    if (ptr == NULL)
    {
        return malloc(size);
    }

    _heap_acquire();
    _heap_header_t* block = (_heap_header_t*)((uint64_t)ptr - sizeof(_heap_header_t));
    if (block->size == ROUND_UP(size, _HEAP_ALIGNMENT))
    {
        _heap_release();
        return ptr;
    }

    void* newPtr = _heap_alloc(size);
    if (newPtr == NULL)
    {
        _heap_release();
        return NULL;
    }
    memcpy(newPtr, ptr, MIN(size, block->size));
    _heap_free(ptr);

    _heap_release();
    return newPtr;
}
