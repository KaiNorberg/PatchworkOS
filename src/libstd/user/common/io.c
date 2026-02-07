#include "io.h"

#include <stdlib.h>
#include <sys/fs.h>
#include <sys/proc.h>
#include <threads.h>

ioring_t _stdIoring;
mtx_t _stdIoringMtx;

void _io_init(void)
{
    status_t status = ioring_setup(&_stdIoring, NULL, 64, 64);
    if (IS_ERR(status))
    {
        exits(F("libstd: failed to setup ioring %Y", status));
    }

    mtx_init(&_stdIoringMtx, mtx_recursive);
}