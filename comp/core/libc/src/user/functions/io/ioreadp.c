#include <libc/fs.h>
#include <libc/io.h>

status_t ioreadp(fd_t cwd, fd_t root, const char* path, const iovec_t* vector, size_t count, ssize_t offset,
    size_t* bytesRead)
{
    iovar_t fdReg = IOREG(IOREG0, FDNONE);
    IOWALKQ(cwd, root, path, IOSOFT, &fdReg, 0);

    iovar_t readReg = IOREG(IOREG1, 0);
    IOREADQ(fdReg, vector, count, offset, IOSOFT, &readReg, 0);

    IODROPQ(fdReg, IONOLINK, NULL, 0);

    status_t status = iosync();
    if (IS_ERR(status))
    {
        return status;
    }

    if (bytesRead != NULL)
    {
        *bytesRead = IOREG_LOAD(readReg);
    }

    return status;
}
