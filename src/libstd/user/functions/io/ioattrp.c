#include <sys/io.h>

status_t ioattrp(fd_t fd, const char* path, vattr_t attr, uint64_t* value)
{
    if (value == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

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
    ioprep_attr(sqe, (IOSQE_REG0 << IOSQE_LOAD0) | IOSQE_LINK, CLOCKS_NEVER, 0, FDNONE, attr, *value);
    iosqe_put(&_stdIoring);

    sqe = iosqe_get(&_stdIoring);
    ioprep_drop(sqe, (IOSQE_REG0 << IOSQE_LOAD0) | IOSQE_LINK, CLOCKS_NEVER, 0, FDNONE);
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
    *value = cqe->result;
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