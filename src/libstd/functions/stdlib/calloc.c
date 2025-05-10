#include <stdlib.h>
#include <string.h>

#include "common/heap.h"

void* calloc(size_t nmemb, size_t size)
{
    _HeapAcquire();
    void* ptr = _HeapAlloc(nmemb * size);
    _HeapRelease();
    memset(ptr, 0, nmemb * size);
    return ptr;
}
