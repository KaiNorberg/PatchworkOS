#include "user/common/threading.h"

#include <libc/fs.h>
#include <libc/io.h>
#include <libc/proc.h>
#include <stdlib.h>
#include <threads.h>

ioring_t* _ioring_get(void)
{
    _thread_t* self = _THREAD_SELF->self;
    if (self->activeRing != NULL)
    {
        return self->activeRing;
    }

    if (!self->hasRing)
    {
        status_t status = ioring_setup(&self->ring, NULL, 64, 64);
        if (IS_ERR(status))
        {
            proc_exit(IOFMT("libc: failed to setup ioring %Y", status));
        }
        self->hasRing = true;
    }
    return &self->ring;
}