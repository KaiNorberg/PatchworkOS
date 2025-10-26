#include <stdlib.h>

#include "common/heap.h"

void* malloc(size_t size)
{
    _heap_acquire();

    _heap_header_t* block = _heap_alloc(size);
    if (block == NULL)
    {
        _heap_release();
        return NULL;
    }

    // When this function returns we have no way of knowing whether the caller
    // will fill the memory or not, so we clear the zeroed flag.
    block->flags &= ~_HEAP_ZEROED;

    _heap_release();
    return block->data;
}
