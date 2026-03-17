#include <errno.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <sys/elf.h>
#include <sys/fs.h>
#include <sys/io.h>
#include <sys/proc.h>

#define INTERPRETER_BASE 0x700000000000

static uint64_t setup_stack(void* stack, uint64_t stackSize, uint64_t stackVaddr, proc_args_t args, const auxv_t* auxv)
{
    size_t argc = 0;
    for (size_t i = 0; i < args.len; i++)
    {
        if (args.buf[i] == '\0')
        {
            argc++;
        }
    }

    uintptr_t spOffset = stackSize;

    spOffset -= args.len;
    void* strData = (uint8_t*)stack + spOffset;
    memcpy(strData, args.buf, args.len);

    uintptr_t* argv = malloc(argc * sizeof(uintptr_t));
    if (argv == NULL)
    {
        return 0;
    }

    uintptr_t currentStr = stackVaddr + spOffset;
    size_t argIdx = 0;
    for (size_t i = 0; i < args.len; i++)
    {
        if (i == 0 || args.buf[i - 1] == '\0')
        {
            argv[argIdx++] = currentStr;
        }
        currentStr++;
    }

    size_t auxvSize = sizeof(auxv_t);
    if (auxv != NULL)
    {
        size_t i = 0;
        while (auxv[i].type != AUXV_NULL)
        {
            i++;
        }
        auxvSize = (i + 1) * sizeof(auxv_t);
    }

    size_t ptrsSize = (argc + 1) * sizeof(uintptr_t) + sizeof(uintptr_t) + auxvSize;
    spOffset -= ptrsSize;
    spOffset &= ~15ULL;

    uintptr_t* stackPtrs = (uintptr_t*)((uint8_t*)stack + spOffset);
    size_t pIdx = 0;
    stackPtrs[pIdx++] = argc;
    for (size_t i = 0; i < argc; i++)
    {
        stackPtrs[pIdx++] = argv[i];
    }
    stackPtrs[pIdx++] = 0;

    if (auxv != NULL)
    {
        size_t i = 0;
        while (auxv[i].type != AUXV_NULL)
        {
            stackPtrs[pIdx++] = auxv[i].type;
            stackPtrs[pIdx++] = auxv[i].value;
            i++;
        }
    }

    stackPtrs[pIdx++] = AUXV_NULL;
    stackPtrs[pIdx++] = 0;

    free(argv);

    return stackVaddr + spOffset;
}

static void build_ctl_string(char* ctlStr, const proc_fd_t* fds, size_t count, uint64_t entry, uint64_t initialSp, fd_t extraParentFd, fd_t extraChildFd)
{
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

    if (extraParentFd != FDNONE)
    {
        memcpy(p, "give ", 5);
        p += 5;
        lltoa(extraParentFd, p, 10);
        p += strlen(p);
        *p++ = ' ';
        lltoa(extraChildFd, p, 10);
        p += strlen(p);
        *p++ = '\n';
    }

    memcpy(p, "start ", 6);
    p += 6;
    lltoa(entry, p, 10);
    p += strlen(p);
    *p++ = ' ';
    lltoa(initialSp, p, 10);
    p += strlen(p);
    *p++ = '\n';
    *p = '\0';
}

static status_t load_elf(fd_t cwd, fd_t root, const char* path, fd_t* fdOut, void** sourceOut, size_t* sizeOut, Elf64_File* elfOut)
{
    iovar_t fdReg = IOREG(IOREG0, FDNONE);
    iovar_t sizeReg = IOREG(IOREG1, SIZE_MAX);
    iovar_t sourceReg = IOREG(IOREG2, NULL);

    IOWALKQ(cwd, root, path, IOSOFT, &fdReg, 0);
    IOATTRQ(fdReg, FILE_GET_SIZE, 0, IOHARD, &sizeReg, 0);
    IOMAPQ(fdReg, NULL, sizeReg, 0, IOMAP_READ, IONOLINK, &sourceReg, 0);
    
    status_t status = iosync();

    *fdOut = (fd_t)IOREG_LOAD(fdReg);
    *sourceOut = (void*)IOREG_LOAD(sourceReg);
    *sizeOut = IOREG_LOAD(sizeReg);

    if (IS_ERR(status))
    {
        if (*sourceOut != NULL)
        {
            iounmap(*sourceOut, *sizeOut);
        }
        if (*fdOut != FDNONE)
        {
            iodrop(*fdOut);
        }
        return status;
    }

    uint64_t result = elf64_validate(elfOut, *sourceOut, *sizeOut);
    if (result != 0)
    {
        iounmap(*sourceOut, *sizeOut);
        iodrop(*fdOut);
        return ERR(LIBSTD, INVALELF);
    }

    return OK;
}

static status_t allocate_process_memory(fd_t cwd, fd_t root, size_t destSize, Elf64_Addr targetVaddr, size_t stackSize, Elf64_Addr stackVaddr, fd_t* procFdOut, void** destOut, void** stackOut)
{
    iovar_t memReg = IOREG(IOREG1, FDNONE);
    iovar_t destReg = IOREG(IOREG2, NULL);
    iovar_t procReg = IOREG(IOREG3, FDNONE);
    iovar_t stackReg = IOREG(IOREG4, NULL);

    IOWALKQ(cwd, root, "/proc/clone:rwx", IOSOFT, &procReg, 0);
    IOWALKQ(procReg, root, "mem:rwx", IOSOFT, &memReg, 0);
    IOMAPQ(memReg, NULL, destSize, targetVaddr, IOMAP_READ | IOMAP_WRITE | IOMAP_EXEC, IOHARD, &destReg, 0);
    IOMAPQ(memReg, NULL, stackSize, stackVaddr, IOMAP_READ | IOMAP_WRITE, IOHARD, &stackReg, 0);
    IODROPQ(memReg, IONOLINK, NULL, 0);

    status_t status = iosync();

    *procFdOut = (fd_t)IOREG_LOAD(procReg);
    *destOut = (void*)IOREG_LOAD(destReg);
    *stackOut = (void*)IOREG_LOAD(stackReg);

    if (IS_ERR(status))
    {
        if (*procFdOut != FDNONE)
        {
            iodrop(*procFdOut);
        }
        if (*destOut != NULL)
        {
            iounmap(*destOut, destSize);
        }
        if (*stackOut != NULL)
        {
            iounmap(*stackOut, stackSize);
        }
        return status;
    }

    return OK;
}

status_t proc_create(fd_t cwd, fd_t root, proc_args_t args, const proc_fd_t* fds, size_t count, prio_t priority, proc_flags_t flags,
    fd_t* proc)
{
    UNUSED(flags); ///< @todo Handle flags within proc_create().

    if (args.buf == NULL || args.len == 0 || args.buf[args.len - 1] != '\0' || (fds == NULL && count != 0))
    {
        return ERR(LIBSTD, INVAL);
    }

    if (args.len > PAGE_SIZE * 8 || count > (PAGE_SIZE - 128) / 64)
    {
        return ERR(LIBSTD, TOOBIG);
    }

    fd_t exeFd = FDNONE;
    void* source = NULL;
    size_t sourceSize = 0;
    Elf64_File elf;
    status_t status = load_elf(cwd, root, args.buf, &exeFd, &source, &sourceSize, &elf);
    if (IS_ERR(status))
    {
        return status;
    }

    Elf64_File interpElf;
    void* interpSource = NULL;
    size_t interpSize = 0;
    
    fd_t exeChildFd = FDNONE;

    auxv_t auxv[8];
    auxv_t* pAuxv = NULL;

    if (elf.interp != NULL)
    {
        fd_t interpFd = FDNONE;
        status = load_elf(cwd, root, elf.interp, &interpFd, &interpSource, &interpSize, &interpElf);
        if (IS_ERR(status))
        {
            if (IS_CODE(status, NOENT))
            {
                status = ERR(LIBSTD, NOINTERP);
            }

            goto cleanup_source;
        }

        iodrop(interpFd);

        exeChildFd = 3;
        if (fds != NULL)
        {
            for (size_t i = 0; i < count; i++)
            {
                if (fds[i].child >= exeChildFd)
                {
                    exeChildFd = fds[i].child + 1;
                }
            }
        }

        size_t auxvCount = 0;
        auxv[auxvCount++] = (auxv_t){.type = AUXV_EXECFD, .value = exeChildFd};
        auxv[auxvCount++] = (auxv_t){.type = AUXV_ENTRY, .value = elf.header->e_entry};
        auxv[auxvCount++] = (auxv_t){.type = AUXV_BASE, .value = INTERPRETER_BASE};
        auxv[auxvCount++] = (auxv_t){.type = AUXV_NULL, .value = 0};
        pAuxv = auxv;

        elf = interpElf;
    }

    Elf64_Addr minAddr = UINT64_MAX;
    Elf64_Addr maxAddr = 0;
    elf64_get_loadable_bounds(&elf, &minAddr, &maxAddr);
    if (minAddr > maxAddr)
    {
        status = ERR(LIBSTD, INVALELF);
        goto cleanup_interp;
    }
    uint64_t destSize = maxAddr - minAddr;

    Elf64_Addr targetVaddr = minAddr;
    if (interpSource != NULL)
    {
        targetVaddr = INTERPRETER_BASE;
    }

    uint64_t stackSize = PAGE_SIZE * 16;
    uint64_t stackVaddr = 0x7FFFFFFF0000 - stackSize;

    fd_t procFd = FDNONE;
    void* dest = NULL;
    void* stack = NULL;

    status = allocate_process_memory(cwd, root, destSize, targetVaddr, stackSize, stackVaddr, &procFd, &dest, &stack);
    if (IS_ERR(status))
    {
        goto cleanup_interp;
    }

    elf64_load_segments(&elf, (Elf64_Addr)dest, (Elf64_Addr)minAddr);

    uint64_t initialSp = setup_stack(stack, stackSize, stackVaddr, args, pAuxv);
    if (initialSp == 0)
    {
        status = ERR(LIBSTD, NOMEM);
        goto cleanup_proc;
    }

    uint64_t entry = elf.header->e_entry - minAddr + targetVaddr;

    if (interpSource != NULL)
    {
        iounmap(interpSource, interpSize);
    }
    iounmap(source, sourceSize);
    iounmap(dest, destSize);
    iounmap(stack, stackSize);

    char prioStr[MAX_NAME];
    iovar_t prioReg = IOREG(IOREG4, FDNONE);
    if (priority != PRIO_DEFAULT)
    {
        lltoa(priority, prioStr, 10);

        IOWALKQ(procFd, root, "prio", IOSOFT, &prioReg, 0);
        IOWRITEQ(prioReg, IOBUF(prioStr, strlen(prioStr)), 0, IOHARD, NULL, 0);
        IODROPQ(prioReg, IONOLINK, NULL, 0);
    }    

    char ctlStr[PAGE_SIZE];
    build_ctl_string(ctlStr, fds, count, entry, initialSp, exeChildFd != FDNONE ? exeFd : FDNONE, exeChildFd);

    iovar_t ctlReg = IOREG(IOREG6, FDNONE);

    IOWALKQ(procFd, root, "ctl", IOSOFT, &ctlReg, 0);
    IOWRITEQ(ctlReg, IOBUF(ctlStr, strlen(ctlStr)), 0, IOHARD, NULL, 0);
    IODROPQ(ctlReg, IOHARD, NULL, 0);

    IODROPQ(exeFd, IONOLINK, NULL, 0);

    status = iosync();

    if (IS_ERR(status))
    {
        iodrop(procFd);
        return status;
    }

    if (proc != NULL)
    {
        *proc = procFd;
    }
    else
    {
        iodrop(procFd);
    }

    return OK;

cleanup_proc:
    if (procFd != FDNONE)
    {
        iodrop(procFd);
    }
    if (dest != NULL)
    {
        iounmap(dest, destSize);
    }
    if (stack != NULL)
    {
        iounmap(stack, stackSize);
    }
cleanup_interp:
    if (interpSource != NULL)
    {
        iounmap(interpSource, interpSize);
    }
cleanup_source:
    if (source != NULL)
    {
        iounmap(source, sourceSize);
    }
    if (exeFd != FDNONE)
    {
        iodrop(exeFd);
    }
    
    return status;
}
