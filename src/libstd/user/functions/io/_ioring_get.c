#include "user/common/threading.h"

#include <stdlib.h>
#include <sys/fs.h>
#include <sys/proc.h>
#include <sys/io.h>
#include <threads.h>

ioring_t* _ioring_get(void)
{
    _thread_t* self = _THREAD_SELF->self;
    if (self->ring == NULL)
    {
        self->ring = malloc(sizeof(ioring_t));
        if (self->ring == NULL)
        {
            proc_exit("libstd: failed to allocate ioring");
        }
        status_t status = ioring_setup(self->ring, NULL, 64, 64);
        if (IS_ERR(status))
        {
            proc_exit(IOFMT("libstd: failed to setup ioring %Y", status));
        }
    }
    return self->ring;
}