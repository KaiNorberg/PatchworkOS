#include "io.h"

#include <stdlib.h>
#include <threads.h>

ioring_t _stdIoring;
mtx_t _stdIoringMtx;

void _io_init(void)
{
    status_t status = ioring_setup(&_stdIoring, NULL, 64, 64);
    if (IS_ERR(status))
    {
        printf("failed to setup ioring %Y", status);
        abort();
    }

    mtx_init(&_stdIoringMtx, mtx_recursive);
}