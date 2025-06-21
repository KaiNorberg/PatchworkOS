#include <stdlib.h>

#include "platform/user/common/heap.h"

void free(void* ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    _heap_acquire();
    _heap_free(ptr);
    _heap_release();
}
