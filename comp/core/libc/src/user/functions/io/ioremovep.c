#include <libc/fs.h>
#include <libc/io.h>

status_t ioremovep(fd_t cwd, fd_t root, const char* path)
{
    iovar_t fdReg = IOREG(IOREG0, FDNONE);
    IOWALKQ(cwd, root, path, IOSOFT, &fdReg, 0);

    IOREMOVEQ(fdReg, IOSOFT, NULL, 0);

    IODROPQ(fdReg, IONOLINK, NULL, 0);

    return iosync();
}