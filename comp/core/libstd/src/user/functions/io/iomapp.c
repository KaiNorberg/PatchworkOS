#include <libstd/fs.h>
#include <libstd/io.h>

status_t iomapp(fd_t cwd, fd_t root, const char* path, void** address, size_t count, ssize_t offset, iomap_t mem)
{
    iovar_t fdReg = IOREG(IOREG0, FDNONE);
    IOWALKQ(cwd, root, path, IOSOFT, &fdReg, 0);

    iovar_t addrReg = IOREG(IOREG1, NULL);
    IOMAPQ(fdReg, *address, count, offset, mem, IOHARD, &addrReg, 0);

    IODROPQ(fdReg, IONOLINK, NULL, 0);

    status_t status = iosync();
    if (IS_ERR(status))
    {
        return status;
    }

    *address = (void*)IOREG_LOAD(addrReg);

    return status;
}
