#include <stdlib.h>

#include "common/heap.h"

void* malloc(size_t size)
{
    _HeapAcquire();
    void* ptr = _HeapAlloc(size);
    _HeapRelease();
    return ptr;
}
