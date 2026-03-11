#include <sys/io.h>

status_t ioattrp(fd_t cwd, fd_t root, const char* path, file_attr_t attr, uint64_t* value)
{
    if (value == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    iowalkq(cwd, root, path, 0);
    iolink(IOREG0, IOLINK_SOFT);

    ioattrq(FDNONE, attr, *value, 0);
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

        if (cqe.op == IOOP_ATTR)
        {
            *value = cqe.result;
        }

        if (!IS_ERR(cqe.status))
        {
            status = cqe.status;
        }
    }

    return status;
}