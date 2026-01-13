#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

#include "common/heap.h"

#ifdef _KERNEL_
#include <kernel/log/panic.h>
#else
#include <stdio.h>
#endif

void* realloc(void* ptr, size_t size)
{
    if (ptr == NULL)
    {
        return malloc(size);
    }

    if (size == 0)
    {
        free(ptr);
        return NULL;
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

    uint64_t alignedSize = ROUND_UP(size, _HEAP_ALIGNMENT);

    if (block->flags & _HEAP_MAPPED)
    {
        goto direct_alloc;
    }

    if (alignedSize < block->size)
    {
        uint64_t remainder = block->size - alignedSize;
        if (remainder >= sizeof(_heap_header_t) + _HEAP_ALIGNMENT)
        {
            _heap_block_split(block, alignedSize);
        }

        _heap_release();
        return ptr;
    }

    if (alignedSize > block->size)
    {
        _heap_header_t* next = CONTAINER_OF_SAFE(block->listEntry.next, _heap_header_t, listEntry);

        if (next != NULL && !(next->flags & _HEAP_ALLOCATED) && (block->data + block->size == (uint8_t*)next))
        {
            uint64_t combinedSize = block->size + sizeof(_heap_header_t) + next->size;
            if (combinedSize >= alignedSize && combinedSize <= _HEAP_LARGE_ALLOC_THRESHOLD)
            {
                assert(!(next->flags & _HEAP_MAPPED));
                _heap_remove_from_free_list(next);
                block->size = combinedSize;
                list_remove(&next->listEntry);

                uint64_t remainder = combinedSize - alignedSize;
                if (remainder >= sizeof(_heap_header_t) + _HEAP_ALIGNMENT)
                {
                    _heap_block_split(block, alignedSize);
                }

                _heap_release();
                return ptr;
            }
        }
    }

direct_alloc:
    _heap_release();
    void* newPtr = malloc(size);
    if (newPtr == NULL)
    {
        return NULL;
    }
    memcpy(newPtr, ptr, MIN(block->size, size));
    free(ptr);
    return newPtr;
}
