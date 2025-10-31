#include <stdlib.h>

#include "common/heap.h"

#ifdef _KERNEL_
#include <kernel/log/panic.h>
#else
#include <stdio.h>
#endif

void free(void* ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    _heap_acquire();

    _heap_header_t* block = CONTAINER_OF(ptr, _heap_header_t, data);

    if (block->magic != _HEAP_HEADER_MAGIC)
    {
#ifdef _KERNEL_
        panic(NULL, "heap corruption detected in free()");
#else
        printf("heap corruption detected in free()\n");
        abort();
#endif
    }

    if (!(block->flags & _HEAP_ALLOCATED))
    {
#ifdef _KERNEL_
        panic(NULL, "double free detected in free()");
#else
        printf("double free detected in free()\n");
        abort();
#endif
    }

    _heap_free(block);
    _heap_release();
}
