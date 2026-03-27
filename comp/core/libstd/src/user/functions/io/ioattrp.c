#include <libstd/io.h>

status_t ioattrp(fd_t cwd, fd_t root, const char* path, file_attr_t attr, uint64_t* value)
{
    if (value == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    iovar_t fdReg = IOREG(IOREG0, FDNONE);
    IOWALKQ(cwd, root, path, IOSOFT, &fdReg, 0);
    IOATTRQ(fdReg, attr, *value, IOSOFT, NULL, 0);
    IODROPQ(fdReg, IONOLINK, NULL, 0);

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