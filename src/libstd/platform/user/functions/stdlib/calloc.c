#include <stdlib.h>
#include <string.h>

#include "platform/user/common/heap.h"

void* calloc(size_t nmemb, size_t size)
{
    _heap_acquire();
    void* ptr = _heap_alloc(nmemb * size);

    _heap_release();
    if (ptr == NULL)
    {
        return NULL;
    }

    memset(ptr, 0, nmemb * size);
    return ptr;
}
