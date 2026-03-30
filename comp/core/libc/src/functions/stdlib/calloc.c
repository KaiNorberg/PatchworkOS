#include <stdlib.h>
#include <string.h>

#include "common/heap.h"

#ifdef _KERNEL_
#include <kernel/log/panic.h>
#else
#include <stdio.h>
#endif

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

    if (block->magic != _HEAP_HEADER_MAGIC)
    {
#ifdef _KERNEL_
        panic(NULL, "heap corruption detected in calloc()");
#else
        proc_exit("libc: heap corruption detected in calloc()");
#endif
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
