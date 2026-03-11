#include <sys/io.h>

status_t iowritep(fd_t cwd, fd_t root, const char* path, const iovec_t* vector, size_t count, ssize_t offset,
    size_t* bytesWritten)
{
    iowalkq(cwd, root, path, 0);
    iolink(IOREG0, IOLINK_SOFT);

    iowriteq(FDNONE, vector, count, offset, 0);
    iouse(IOARG0, IOREG0);
    iolink(IOREG_NONE, IOLINK_HARD);

    iodropq(FDNONE, 0);
    iouse(IOARG0, IOREG0);

    status_t status = OK;
    iocqe_t cqe = {0};

    for (int i = 0; i < 3; i++)
    {
        status = iowait(&cqe);
        if (IS_ERR(status))
        {
            return status;
        }

        if (cqe.op == IOOP_WRITE)
        {
            if (bytesWritten != NULL)
            {
                *bytesWritten = cqe.result;
            }
        }

        if (!IS_ERR(cqe.status))
        {
            status = cqe.status;
        }
    }

    return status;
}
