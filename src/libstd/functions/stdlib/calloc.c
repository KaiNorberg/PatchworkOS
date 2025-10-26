#include <stdlib.h>
#include <string.h>

#include "common/heap.h"

void* calloc(size_t nmemb, size_t size)
{
    size_t totalSize = nmemb * size;
    if (size != 0 && totalSize / size != nmemb)
    {
        return NULL;
    }

    _heap_acquire();

    _heap_header_t* block = _heap_alloc(totalSize);
    if (block == NULL)
    {
        _heap_release();
        return NULL;
    }

    if (!(block->flags & _HEAP_ZEROED))
    {
        memset(block->data, 0, totalSize);
    }
    // When this function returns we have no way of knowing whether the caller
    // will fill the memory or not, so we clear the zeroed flag.
    block->flags &= ~_HEAP_ZEROED;

    _heap_release();
    return block->data;
}
