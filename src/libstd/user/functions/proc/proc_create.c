#include <errno.h>
#include <sys/elf.h>
#include <sys/fs.h>
#include <sys/io.h>
#include <sys/proc.h>

status_t proc_create(proc_args_t args, const proc_fd_t* fds, size_t count, prio_t priority, proc_flags_t flags,
    fd_t* proc)
{
    UNUSED(flags);

    if (args.buf == NULL || args.len == 0 || (fds == NULL && count != 0))
    {
        return ERR(LIBSTD, INVAL);
    }

    iovar_t exeReg = IOREG(IOREG0, FDNONE);
    iovar_t sizeReg = IOREG(IOREG1, SIZE_MAX);
    iovar_t sourceReg = IOREG(IOREG2, NULL);
    iovar_t procReg = IOREG(IOREG3, FDNONE);

    IOWALKQ(FDCWD, FDROOT, args.buf, IOSOFT, &exeReg, 0);
    IOATTRQ(exeReg, FILE_GET_SIZE, 0, IOHARD, &sizeReg, 0);
    IOMAPQ(exeReg, NULL, sizeReg, 0, IOMAP_READ, IOHARD, &sourceReg, 0);
    IODROPQ(exeReg, IOSOFT, NULL, 0);
    
    IOWALKQ(FDCWD, FDROOT, "/proc/clone:rwx", IONOLINK, &procReg, 0);

    status_t status = iosync();

    void* source = (void*)IOREG_LOAD(sourceReg);
    size_t sourceSize = IOREG_LOAD(sizeReg);

    if (IS_ERR(status))
    {
        if (source != NULL)
        {
            iounmap(source, sourceSize);
        }
        return status;
    }

    Elf64_File elf;
    uint64_t result = elf64_validate(&elf, source, sourceSize);
    if (result != 0)
    {
        iounmap(source, sourceSize);
        iodrop(IOREG_LOAD(procReg));
        return ERR(LIBSTD, INVALELF);
    }

    Elf64_Addr minAddr = UINT64_MAX;
    Elf64_Addr maxAddr = 0;
    elf64_get_loadable_bounds(&elf, &minAddr, &maxAddr);
    uint64_t destSize = maxAddr - minAddr;

    iovar_t memReg = IOREG(exeReg, FDNONE);
    iovar_t destReg = IOREG(sizeReg, NULL);

    IOWALKQ(procReg, FDROOT, "mem:rwx", IOSOFT, &memReg, 0);
    IOMAPQ(memReg, NULL, destSize, minAddr, IOMAP_READ | IOMAP_WRITE | IOMAP_EXEC, IOHARD, &destReg, 0);
    IODROPQ(memReg, IONOLINK, NULL, 0);

    iovar_t prioReg = IOREG(IOREG4, FDNONE);
    if (priority != PRIO_DEFAULT)
    {
        char prioStr[MAX_NAME];
        lltoa(priority, prioStr, 10);

        IOWALKQ(procReg, FDROOT, "prio", IOSOFT, &prioReg, 0);
        IOWRITEQ(prioReg, IOBUF(prioStr, strlen(prioStr)), 0, IOHARD, NULL, 0);
        IODROPQ(prioReg, IONOLINK, NULL, 0);
    }

    char ctlStr[PAGE_SIZE];
    char* p = ctlStr;
    if (fds != NULL)
    {
        for (size_t i = 0; i < count; i++)
        {
            memcpy(p, "give ", 5);
            p += 5;
            lltoa(fds[i].parent, p, 10);
            p += strlen(p);
            *p++ = ' ';
            lltoa(fds[i].child, p, 10);
            p += strlen(p);
            *p++ = '\n';
        }
    }
    memcpy(p, "start ", 6);
    p += 6;
    lltoa(elf.header->e_entry, p, 10);
    p += strlen(p);
    *p++ = '\n';
    *p = '\0';

    iovar_t ctlReg = IOREG(IOREG5, FDNONE);
    iovar_t cmdlineReg = IOREG(IOREG6, FDNONE);

    IOWALKQ(procReg, FDROOT, "ctl", IOSOFT, &ctlReg, 0);
    IOWRITEQ(ctlReg, IOBUF(ctlStr, strlen(ctlStr)), 0, IOHARD, NULL, 0);
    IODROPQ(ctlReg, IONOLINK, NULL, 0);

    IOWALKQ(procReg, FDROOT, "cmdline", IOSOFT, &cmdlineReg, 0);
    IOWRITEQ(cmdlineReg, IOBUF(args.buf, args.len), 0, IOHARD, NULL, 0);
    IODROPQ(cmdlineReg, IONOLINK, NULL, 0);

    status = iosync();

    void* dest = (void*)IOREG_LOAD(destReg);

    if (IS_ERR(status))
    {
        iodrop(IOREG_LOAD(procReg));

        iounmap(source, sourceSize);
        iounmap(dest, destSize);
        return status;
    }

    elf64_load_segments(&elf, (Elf64_Addr)dest, (Elf64_Addr)minAddr);
    iounmap(source, sourceSize);
    iounmap(dest, destSize);

    if (proc != NULL)
    {
        *proc = IOREG_LOAD(procReg);
    }
    else
    {
        iodrop(IOREG_LOAD(procReg));
    }

    return OK;
}
