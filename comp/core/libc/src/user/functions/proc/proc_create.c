#include <elf.h>
#include <errno.h>
#include <libc/fs.h>
#include <libc/io.h>
#include <libc/proc.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/auxv.h>

#define INTERPRETER_BASE 0x700000000000

static uint64_t setup_stack(void* stack, uint64_t stackSize, uint64_t stackVaddr, proc_args_t args, proc_envp_t envp, const auxv_t* auxv)
{
    size_t argc = 0;
    for (size_t i = 0; i < args.len; i++)
    {
        if (args.buf[i] == '\0')
        {
            argc++;
        }
    }

    size_t envc = 0;
    for (size_t i = 0; i < envp.len; i++)
    {
        if (envp.buf[i] == '\0')
        {
            envc++;
        }
    }

    uintptr_t spOffset = stackSize;

    spOffset -= envp.len;
    void* envStrData = (uint8_t*)stack + spOffset;
    memcpy(envStrData, envp.buf, envp.len);

    spOffset -= args.len;
    void* strData = (uint8_t*)stack + spOffset;
    memcpy(strData, args.buf, args.len);

    uintptr_t* argv = malloc((argc + envc) * sizeof(uintptr_t));
    if (argv == NULL) return 0;

    uintptr_t* envv = argv + argc;

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

    uintptr_t currentEnvStr = stackVaddr + spOffset + args.len;
    size_t envIdx = 0;
    for (size_t i = 0; i < envp.len; i++)
    {
        if (i == 0 || envp.buf[i - 1] == '\0')
        {
            envv[envIdx++] = currentEnvStr;
        }
        currentEnvStr++;
    }

    size_t auxvSize = sizeof(auxv_t);
    if (auxv != NULL)
    {
        size_t i = 0;
        while (auxv[i].a_type != AT_NULL)
        {
            i++;
        }
        auxvSize = (i + 1) * sizeof(auxv_t);
    }

    size_t ptrsSize = sizeof(uintptr_t) + (argc + 1) * sizeof(uintptr_t) + (envc + 1) * sizeof(uintptr_t) + auxvSize;
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

    for (size_t i = 0; i < envc; i++)
    {
        stackPtrs[pIdx++] = envv[i];
    }
    stackPtrs[pIdx++] = 0;

    if (auxv != NULL)
    {
        size_t i = 0;
        while (auxv[i].a_type != AT_NULL)
        {
            stackPtrs[pIdx++] = auxv[i].a_type;
            stackPtrs[pIdx++] = auxv[i].a_un.a_val;
            i++;
        }
    }

    stackPtrs[pIdx++] = AT_NULL;
    stackPtrs[pIdx++] = 0;

    free(argv);

    return stackVaddr + spOffset;
}

static void build_ctl_string(char* ctlStr, const proc_fd_t* fds, size_t count, uint64_t entry, uint64_t initialSp,
    fd_t extraParentFd, fd_t extraChildFd)
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

static status_t load_elf(fd_t cwd, fd_t root, const char* path, fd_t* fdOut, void** sourceOut, size_t* sizeOut,
    Elf64_File* elfOut)
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
        return status;
    }

    uint64_t result = elf64_validate(elfOut, *sourceOut, *sizeOut);
    if (result != 0)
    {
        return ERR(LIBSTD, INVALELF);
    }

    return OK;
}

static status_t allocate_process_memory(fd_t cwd, fd_t root, size_t destSize, Elf64_Addr targetVaddr, size_t stackSize,
    Elf64_Addr stackVaddr, fd_t* procFdOut, void** destOut, void** stackOut)
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

    return OK;
}

status_t proc_create(fd_t cwd, fd_t root, proc_args_t args, proc_envp_t envp, const proc_fd_t* fds, size_t count, prio_t priority,
    proc_flags_t flags, fd_t* proc)
{
    UNUSED(flags); ///< @todo Handle flags within proc_create().

    status_t status = OK;
    char* envInherit = NULL;
    fd_t exeFd = FDNONE;
    void* source = NULL;
    size_t sourceSize = 0;
    Elf64_File elf;
    Elf64_File interpElf;
    void* interpSource = NULL;
    size_t interpSize = 0;
    fd_t procFd = FDNONE;
    fd_t interpFd = FDNONE;
    void* dest = NULL;
    void* stack = NULL;
    size_t destSize = 0;
    size_t stackSize = 0;

    Elf64_Addr minAddr = UINT64_MAX;
    Elf64_Addr maxAddr = 0;
    Elf64_Addr targetVaddr;
    uintptr_t stackVaddr;
    uint64_t initialSp;
    uint64_t entry;
    fd_t exeChildFd = FDNONE;
    auxv_t auxv[8];
    auxv_t* pAuxv = NULL;
    char prioStr[MAX_NAME];
    char ctlStr[PAGE_SIZE];

    if (args.buf == NULL || args.len == 0 || args.buf[args.len - 1] != '\0' || (fds == NULL && count != 0))
    {
        return ERR(LIBSTD, INVAL);
    }

    if (envp.buf != NULL && (envp.len == 0 || envp.buf[envp.len - 1] != '\0'))
    {
        return ERR(LIBSTD, INVAL);
    }

    if (envp.buf == NULL)
    {
        size_t len = 0;
        if (environ != NULL)
        {
            for (char** env = environ; *env != NULL; env++)
            {
                len += strlen(*env) + 1;
            }
        }

        if (len > 0)
        {
            envInherit = malloc(len);
            if (envInherit == NULL)
            {
                status = ERR(LIBSTD, NOMEM);
                goto cleanup;
            }

            char* ptr = envInherit;
            for (char** env = environ; *env != NULL; env++)
            {
                size_t l = strlen(*env);
                memcpy(ptr, *env, l);
                ptr[l] = '\0';
                ptr += l + 1;
            }
            envp.buf = envInherit;
            envp.len = len;
        }
    }

    if (args.len > PAGE_SIZE * 8 || envp.len > PAGE_SIZE * 8 || count > (PAGE_SIZE - 128) / 64)
    {
        status = ERR(LIBSTD, TOOBIG);
        goto cleanup;
    }

    status = load_elf(cwd, root, args.buf, &exeFd, &source, &sourceSize, &elf);
    if (IS_ERR(status))
    {
        goto cleanup;
    }

    if (elf.interp != NULL)
    {
        status = load_elf(cwd, root, elf.interp, &interpFd, &interpSource, &interpSize, &interpElf);
        if (IS_ERR(status))
        {
            if (IS_CODE(status, NOENT))
            {
                status = ERR(LIBSTD, NOINTERP);
            }

            goto cleanup;
        }

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
        auxv[auxvCount++] = (auxv_t){.a_type = AT_EXECFD, .a_un.a_val = exeChildFd};
        auxv[auxvCount++] = (auxv_t){.a_type = AT_BASE, .a_un.a_val = INTERPRETER_BASE};
        auxv[auxvCount++] = (auxv_t){.a_type = AT_NULL, .a_un.a_val = 0};
        pAuxv = auxv;

        elf = interpElf;
    }

    elf64_get_loadable_bounds(&elf, &minAddr, &maxAddr);
    if (minAddr > maxAddr)
    {
        status = ERR(LIBSTD, INVALELF);
        goto cleanup;
    }
    destSize = maxAddr - minAddr;

    targetVaddr = (interpSource != NULL) ? INTERPRETER_BASE : minAddr;

    stackSize = PAGE_SIZE * 16;
    stackVaddr = 0x7FFFFFFF0000 - stackSize;

    status = allocate_process_memory(cwd, root, destSize, targetVaddr, stackSize, stackVaddr, &procFd, &dest, &stack);
    if (IS_ERR(status))
    {
        goto cleanup;
    }

    elf64_load_segments(&elf, (Elf64_Addr)dest, (Elf64_Addr)minAddr);

    initialSp = setup_stack(stack, stackSize, stackVaddr, args, envp, pAuxv);
    if (initialSp == 0)
    {
        status = ERR(LIBSTD, NOMEM);
        goto cleanup;
    }

    entry = elf.header->e_entry - minAddr + targetVaddr;

    iovar_t prioReg = IOREG(IOREG4, FDNONE);
    if (priority != PRIO_DEFAULT)
    {
        lltoa(priority, prioStr, 10);

        IOWALKQ(procFd, root, "prio", IOSOFT, &prioReg, 0);
        IOWRITEQ(prioReg, IOBUF(prioStr, strlen(prioStr)), 0, IOHARD, NULL, 0);
        IODROPQ(prioReg, IONOLINK, NULL, 0);
    }

    build_ctl_string(ctlStr, fds, count, entry, initialSp, exeChildFd != FDNONE ? exeFd : FDNONE, exeChildFd);

    iovar_t ctlReg = IOREG(IOREG6, FDNONE);

    IOWALKQ(procFd, root, "ctl", IOSOFT, &ctlReg, 0);
    IOWRITEQ(ctlReg, IOBUF(ctlStr, strlen(ctlStr)), 0, IOHARD, NULL, 0);
    IODROPQ(ctlReg, IOHARD, NULL, 0);

    status = iosync();
    if (IS_ERR(status))
    {
        goto cleanup;
    }

    if (proc != NULL)
    {
        *proc = procFd;
        procFd = FDNONE;
    }

cleanup:
    if (envInherit != NULL)
    {
        free(envInherit);
        envInherit = NULL;
    }
    if (exeFd != FDNONE)
    {
        iodrop(exeFd);
        exeFd = FDNONE;
    }
    if (source != NULL)
    {
        iounmap(source, sourceSize);
        source = NULL;
    }
    if (interpSource != NULL)
    {
        iounmap(interpSource, interpSize);
        interpSource = NULL;
    }
    if (dest != NULL)
    {
        iounmap(dest, destSize);
        dest = NULL;
    }
    if (stack != NULL)
    {
        iounmap(stack, stackSize);
        stack = NULL;
    }
    if (procFd != FDNONE)
    {
        iodrop(procFd);
        procFd = FDNONE;
    }
    if (interpFd != FDNONE)
    {
        iodrop(interpFd);
        interpFd = FDNONE;
    }

    return status;
}
