#include "user/common/threading.h"
#include <libc/io.h>
#include <stdlib.h>

char* getenv(const char* name)
{
    if (name == NULL)
    {
        return NULL;
    }

    _thread_t* thread = _THREAD_SELF->self;
    if (thread->envValue != NULL)
    {
        free(thread->envValue);
        thread->envValue = NULL;
    }

    size_t len = 0;
    if (IS_ERR(ioloadp(FDCWD, FDROOT, IOFMT("/env/%s", name), &thread->envValue, &len)))
    {
        return NULL;
    }

    return thread->envValue;
}