#include <stdlib.h>

#include "common/heap.h"

#ifdef _KERNEL_
#include <kernel/log/panic.h>
#else
#include <stdio.h>
#endif

void* malloc(size_t size)
{
    _heap_acquire();

    _heap_header_t* block = _heap_alloc(size);
    if (block == NULL)
    {
        _heap_release();
        return NULL;
    }

    if (block->magic != _HEAP_HEADER_MAGIC)
    {
#ifdef _KERNEL_
        panic(NULL, "heap corruption detected in malloc()");
#else
        proc_exit("libstd: heap corruption detected in malloc()");
#endif
    }

    // When this function returns we have no way of knowing whether the caller
    // will fill the memory or not, so we clear the zeroed flag.
    block->flags &= ~_HEAP_ZEROED;

    _heap_release();
    return block->data;
}
