#include <stdint.h>
#include <stdlib.h>

#include "libs/std/internal/heap.h"

#ifdef __KERNEL__
#include "debug.h"
#endif

void free(void* ptr)
{
    _HeapAcquire();

    heap_header_t* block = (heap_header_t*)((uint64_t)ptr - sizeof(heap_header_t));
#ifdef __KERNEL__
    if (block->magic != HEAP_HEADER_MAGIC)
    {
        debug_panic("Invalid heap magic\n");
    }
    else if (!block->reserved)
    {
        debug_panic("Attempt to free unreserved block");
    }
#endif
    block->reserved = false;

    _HeapRelease();
}