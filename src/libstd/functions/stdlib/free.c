#include <stdlib.h>

#include "common/heap.h"

void free(void* ptr)
{
    _HeapAcquire();
    _HeapFree(ptr);
    _HeapRelease();
}
