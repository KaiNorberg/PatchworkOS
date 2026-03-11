#include <sys/io.h>

status_t ioreadp(fd_t cwd, fd_t root, const char* path, const iovec_t* vector, size_t count, ssize_t offset,
    size_t* bytesRead)
{
    iowalkq(cwd, root, path, 0);
    iolink(IOREG0, IOLINK_SOFT);

    ioreadq(FDNONE, vector, count, offset, 0);
    iouse(IOARG0, IOREG0);
    iolink(IOREG_NONE, IOLINK_SOFT);

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

        if (cqe.op == IOOP_READ)
        {
            if (bytesRead != NULL)
            {
                *bytesRead = cqe.result;
            }
        }

        if (!IS_ERR(cqe.status))
        {
            status = cqe.status;
        }
    }

    return status;
}
