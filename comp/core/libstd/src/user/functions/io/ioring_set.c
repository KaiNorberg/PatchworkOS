#include "user/common/threading.h"

#include <stdlib.h>
#include <sys/fs.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>

ioring_t* ioring_set(ioring_t* ring)
{
    _thread_t* self = _THREAD_SELF->self;
    ioring_t* old = self->activeRing;
    self->activeRing = ring;
    return old;
}