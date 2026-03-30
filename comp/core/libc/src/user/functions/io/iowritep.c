#include <libc/fs.h>
#include <libc/io.h>

status_t iowritep(fd_t cwd, fd_t root, const char* path, const iovec_t* vector, size_t count, ssize_t offset,
    size_t* bytesWritten)
{
    iovar_t fdReg = IOREG(IOREG0, FDNONE);
    IOWALKQ(cwd, root, path, IOSOFT, &fdReg, 0);

    iovar_t writeReg = IOREG(IOREG1, 0);
    IOWRITEQ(fdReg, vector, count, offset, IOHARD, &writeReg, 0);

    IODROPQ(fdReg, IONOLINK, NULL, 0);

    if (bytesWritten != NULL)
    {
        *bytesWritten = IOREG_LOAD(writeReg);
    }

    return iosync();
}
