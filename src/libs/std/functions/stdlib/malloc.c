#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

#include "libs/std/internal/heap.h"

// TODO: Implement correct alignment rules

void* malloc(size_t size)
{
    _HeapAcquire();

    if (size == 0)
    {
        return NULL;
    }
    size = ROUND_UP(size, HEAP_ALIGNMENT);

    heap_header_t* currentBlock = _HeapFirstBlock();
    while (true)
    {
        if (!currentBlock->reserved)
        {
            if (currentBlock->size == size)
            {
                currentBlock->reserved = true;

                _HeapRelease();
                return HEAP_HEADER_GET_START(currentBlock);
            }
            else if (currentBlock->size > size + sizeof(heap_header_t) + HEAP_ALIGNMENT)
            {
                currentBlock->reserved = true;
                _HeapBlockSplit(currentBlock, size);

                _HeapRelease();
                return HEAP_HEADER_GET_START(currentBlock);
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

    heap_header_t* newBlock = _HeapBlockNew(size);
    if (newBlock == NULL)
    {
        return NULL;
    }

    if (newBlock->size > size + sizeof(heap_header_t) + HEAP_ALIGNMENT)
    {
        _HeapBlockSplit(newBlock, size);
    }
    currentBlock->next = newBlock;
    newBlock->reserved = true;

    _HeapRelease();
    return HEAP_HEADER_GET_START(newBlock);
}
