#include <stdlib.h>

#include "platform/user/common/heap.h"

void* malloc(size_t size)
{
    _heap_acquire();
    void* ptr = _heap_alloc(size);
    _heap_release();
    return ptr;
}
