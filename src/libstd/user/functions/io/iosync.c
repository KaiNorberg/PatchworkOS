#include <sys/io.h>

void iosync(iosqe_t* sqe, iocqe_t* cqe)
{
    mtx_lock(&_stdIoringMtx);
    iosqe_t* s = iosqe_get(&_stdIoring);
    if (s == NULL)
    {
        mtx_unlock(&_stdIoringMtx);
        return;
    }
    *s = *sqe;
    iosqe_put(&_stdIoring);
    ioring_enter(&_stdIoring, 1, 1, NULL);
    *cqe = *iocqe_get(&_stdIoring);
    iocqe_put(&_stdIoring);
    mtx_unlock(&_stdIoringMtx);
}