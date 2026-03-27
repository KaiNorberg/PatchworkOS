#include <libstd/fs.h>
#include <libstd/io.h>

status_t ioqueryp(fd_t cwd, fd_t root, const char* path, file_info_t* info)
{
    if (info == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    iovar_t fdReg = IOREG(IOREG0, FDNONE);
    IOWALKQ(cwd, root, path, IOSOFT, &fdReg, 0);

    IOQUERYQ(fdReg, info, IOSOFT, NULL, 0);

    IODROPQ(fdReg, IONOLINK, NULL, 0);

    return iosync();
}