#include <sys/io.h>

status_t ioremovep(fd_t fd, const char* path)
{
    mtx_lock(&_stdIoringMtx);
    if (iosqe_count(&_stdIoring) < 3)
    {
        mtx_unlock(&_stdIoringMtx);
        return ERR(LIBSTD, NOSPACE);
    }

    iosqe_t* sqe = iosqe_get(&_stdIoring);
    ioprep_walk(sqe, (IOSQE_REG0 << IOSQE_SAVE) | IOSQE_LINK, CLOCKS_NEVER, 0, fd, path, strlen(path), NULL, 0);
    iosqe_put(&_stdIoring);

    sqe = iosqe_get(&_stdIoring);
    ioprep_remove(sqe, (IOSQE_REG0 << IOSQE_LOAD0) | IOSQE_LINK, CLOCKS_NEVER, 0, FDNONE);
    iosqe_put(&_stdIoring);

    sqe = iosqe_get(&_stdIoring);
    ioprep_clunk(sqe, (IOSQE_REG0 << IOSQE_LOAD0) | IOSQE_LINK, CLOCKS_NEVER, 0, FDNONE);
    iosqe_put(&_stdIoring);
    
    ioring_enter(&_stdIoring, 3, 3, NULL);

    status_t status = OK;

    iocqe_t* cqe = iocqe_get(&_stdIoring);
    if (cqe->status != OK)
    {
        status = cqe->status;
    }
    iocqe_put(&_stdIoring);
    
    cqe = iocqe_get(&_stdIoring);
    if (cqe->status != OK && status == OK)
    {
        status = cqe->status;
    }
    iocqe_put(&_stdIoring);

    cqe = iocqe_get(&_stdIoring);
    if (cqe->status != OK && status == OK)
    {
        status = cqe->status;
    }
    iocqe_put(&_stdIoring);

    mtx_unlock(&_stdIoringMtx);
    return status;
}