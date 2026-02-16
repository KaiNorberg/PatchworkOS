#include <sys/io.h>

status_t ioreadp(fd_t fd, const char* path, const iovec_t* vector, size_t count, ssize_t offset, size_t* bytesRead)
{
    mtx_lock(&_stdIoringMtx);
    if (iosqe_count(&_stdIoring) < 3)
    {
        mtx_unlock(&_stdIoringMtx);
        return ERR(LIBSTD, NOSPACE);
    }

    iosqe_t* sqe = iosqe_get(&_stdIoring);
    ioprep_open(sqe, (IOSQE_REG0 << IOSQE_SAVE) | IOSQE_LINK, CLOCKS_NEVER, 0, fd, path, strlen(path), NULL, 0);
    iosqe_put(&_stdIoring);

    sqe = iosqe_get(&_stdIoring);
    ioprep_read(sqe, (IOSQE_REG0 << IOSQE_LOAD0) | IOSQE_LINK, CLOCKS_NEVER, 0, FDNONE, vector, count, offset);
    iosqe_put(&_stdIoring);

    sqe = iosqe_get(&_stdIoring);
    ioprep_close(sqe, (IOSQE_REG0 << IOSQE_LOAD0) | IOSQE_LINK, CLOCKS_NEVER, 0, FDNONE);
    iosqe_put(&_stdIoring);
    
    ioring_enter(&_stdIoring, 3, 3, NULL);

    iocqe_get(&_stdIoring);
    iocqe_put(&_stdIoring);

    iocqe_t* cqe = iocqe_get(&_stdIoring);
    if (bytesRead != NULL)
    {
        *bytesRead = cqe->result;
    }
    status_t status = cqe->status;
    iocqe_put(&_stdIoring);

    iocqe_get(&_stdIoring);
    iocqe_put(&_stdIoring);

    mtx_unlock(&_stdIoringMtx);
    return status;
}
