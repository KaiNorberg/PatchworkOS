#include <stdlib.h>

#include "common/heap.h"

void free(void* ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    _HeapAcquire();
    _HeapFree(ptr);
    _HeapRelease();
}
