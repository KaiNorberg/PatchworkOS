#include <kernel/fs/file_table.h>
#include <kernel/sched/loader.h>

#include <kernel/cpu/gdt.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/elf.h>
#include <sys/math.h>

static void* loader_load_program(thread_t* thread)
{
    process_t* process = thread->process;
    space_t* space = &process->space;

    const char* executable = process->argv.buffer[0];
    if (executable == NULL)
    {
        return NULL;
    }

    file_t* file = vfs_open(PATHNAME(executable), process);
    if (file == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(file);

    uint64_t fileSize = vfs_seek(file, 0, SEEK_END);
    vfs_seek(file, 0, SEEK_SET);

    void* fileData = malloc(fileSize);
    if (fileData == NULL)
    {
        return NULL;
    }

    uint64_t readSize = vfs_read(file, fileData, fileSize);
    if (readSize != fileSize)
    {
        free(fileData);
        return NULL;
    }

    Elf64_File elf;
    if (elf64_validate(&elf, fileData, fileSize) != 0)
    {
        free(fileData);
        return NULL;
    }

    Elf64_Addr minAddr = UINT64_MAX;
    Elf64_Addr maxAddr = 0;
    elf64_get_loadable_bounds(&elf, &minAddr, &maxAddr);
    uint64_t loadSize = maxAddr - minAddr;

    if (vmm_alloc(&process->space, (void*)minAddr, loadSize, PML_USER | PML_WRITE | PML_PRESENT, VMM_ALLOC_OVERWRITE) ==
        NULL)
    {
        free(fileData);
        return NULL;
    }

    elf64_load_segments(&elf, 0, 0);

    /*for (uint64_t i = 0; i < elf.header->e_phnum; i++)
    {
        Elf64_Phdr* phdr = ELF64_GET_PHDR(&elf, i);
        if (phdr->p_type != PT_LOAD)
        {
            continue;
        }

        void* segmentData = ELF64_AT_OFFSET(&elf, phdr->p_offset);
        void* destAddr = (void*)phdr->p_vaddr;
        memcpy(destAddr, segmentData, phdr->p_filesz);
        if (phdr->p_memsz > phdr->p_filesz)
        {
            memset((void*)((uint64_t)destAddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
        }

        if (!(phdr->p_flags & PF_W))
        {
            vmm_protect(space, destAddr, phdr->p_memsz, PML_USER | PML_PRESENT);
        }
    }*/

    void* entryPoint = (void*)elf.header->e_entry;
    free(fileData);
    return entryPoint;
}

static char** loader_setup_argv(thread_t* thread)
{
    if (thread->process->argv.size >= PAGE_SIZE)
    {
        return NULL;
    }

    char** argv = memcpy((void*)(thread->userStack.top - sizeof(uint64_t) - thread->process->argv.size),
        thread->process->argv.buffer, thread->process->argv.size);

    for (uint64_t i = 0; i < thread->process->argv.amount; i++)
    {
        argv[i] = (void*)((uint64_t)argv[i] - (uint64_t)thread->process->argv.buffer + (uint64_t)argv);
    }

    return argv;
}

static void loader_process_entry(void)
{
    thread_t* thread = sched_thread();

    void* rip = loader_load_program(thread);
    if (rip == NULL)
    {
        sched_process_exit(ESPAWNFAIL);
    }

    char** argv = loader_setup_argv(thread);
    if (argv == NULL)
    {
        sched_process_exit(ESPAWNFAIL);
    }

    // Disable interrupts, they will be enabled by the IRETQ instruction.
    asm volatile("cli");

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    thread->frame.rdi = thread->process->argv.amount;
    thread->frame.rsi = (uintptr_t)argv;
    thread->frame.rsp = (uintptr_t)ROUND_DOWN((uint64_t)argv - 1, 16);
    thread->frame.rip = (uintptr_t)rip;
    thread->frame.cs = GDT_CS_RING3;
    thread->frame.ss = GDT_SS_RING3;
    thread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    LOG_DEBUG("jump to user space path=%s pid=%d rsp=%p rip=%p\n", thread->process->argv.buffer[0], thread->process->id,
        (void*)thread->frame.rsp, (void*)thread->frame.rip);
    loader_jump_to_user_space(thread);
}

thread_t* loader_spawn(const char** argv, priority_t priority, const path_t* cwd)
{
    process_t* process = sched_process();
    assert(process != NULL);

    if (argv == NULL || argv[0] == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    pathname_t executable;
    if (pathname_init(&executable, argv[0]) == ERR)
    {
        return NULL;
    }

    stat_t info;
    if (vfs_stat(&executable, &info, process) == ERR)
    {
        return NULL;
    }

    if (info.type != INODE_FILE)
    {
        errno = EISDIR;
        return NULL;
    }

    process_t* child = process_new(process, argv, cwd, priority);
    if (child == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(child);

    thread_t* childThread = thread_new(child);
    if (childThread == NULL)
    {
        return NULL;
    }

    childThread->frame.rip = (uintptr_t)loader_process_entry;
    childThread->frame.rsp = childThread->kernelStack.top;
    childThread->frame.cs = GDT_CS_RING0;
    childThread->frame.ss = GDT_SS_RING0;
    childThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    LOG_INFO("spawn path=%s pid=%d\n", argv[0], child->id);
    return childThread;
}

SYSCALL_DEFINE(SYS_SPAWN, pid_t, const char** argv, const spawn_fd_t* fds, const char* cwdString, spawn_attr_t* attr)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    priority_t priority;
    if (attr == NULL || attr->flags & SPAWN_INHERIT_PRIORITY)
    {
        priority = atomic_load(&process->priority);
    }
    else
    {
        priority = attr->priority;
    }

    if (priority >= PRIORITY_MAX_USER)
    {
        errno = EACCES;
        return ERR;
    }

    char** argvCopy;
    uint64_t argc;
    char* argvTerminator = NULL;
    if (thread_copy_from_user_terminated(thread, argv, &argvTerminator, sizeof(char*), CONFIG_MAX_ARGC,
            (void**)&argvCopy, &argc) == ERR)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < argc; i++)
    {
        char* userSpacePtr = argvCopy[i];
        char* kernelStringCopy;
        uint64_t stringLen;
        char terminator = '\0';

        if (thread_copy_from_user_terminated(thread, userSpacePtr, &terminator, sizeof(char), MAX_PATH,
                (void**)&kernelStringCopy, &stringLen) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                free(argvCopy[j]);
            }
            free(argvCopy);
            return ERR;
        }

        argvCopy[i] = kernelStringCopy;
    }

    path_t cwdPath = PATH_EMPTY;
    if (cwdString != NULL)
    {
        pathname_t cwdPathname;
        if (thread_copy_from_user_pathname(thread, &cwdPathname, cwdString) == ERR)
        {
            goto cleanup_argv;
        }

        path_t cwd = cwd_get(&process->cwd);
        PATH_DEFER(&cwd);

        if (path_walk(&cwdPath, &cwdPathname, &cwd, WALK_NONE, &process->ns) == ERR)
        {
            goto cleanup_argv;
        }
    }

    thread_t* child = loader_spawn((const char**)argvCopy, priority, cwdString == NULL ? NULL : &cwdPath);

    for (uint64_t i = 0; i < argc; i++)
    {
        free(argvCopy[i]);
    }
    free((void*)argvCopy);

    if (cwdString != NULL)
    {
        path_put(&cwdPath);
    }

    if (child == NULL)
    {
        return ERR;
    }

    if (fds != NULL)
    {
        spawn_fd_t* fdsCopy;
        uint64_t fdAmount;
        spawn_fd_t fdsTerminator = SPAWN_FD_END;

        if (thread_copy_from_user_terminated(thread, fds, &fdsTerminator, sizeof(spawn_fd_t), CONFIG_MAX_FD,
                (void**)&fdsCopy, &fdAmount) == ERR)
        {
            thread_free(child);
            return ERR;
        }

        for (uint64_t i = 0; i < fdAmount; i++)
        {
            file_t* file = file_table_get(&process->fileTable, fdsCopy[i].parent);
            if (file == NULL)
            {
                free(fdsCopy);
                thread_free(child);
                errno = EBADF;
                return ERR;
            }

            if (file_table_set(&child->process->fileTable, fdsCopy[i].child, file) == ERR)
            {
                DEREF(file);
                free(fdsCopy);
                thread_free(child);
                errno = EBADF;
                return ERR;
            }
            DEREF(file);
        }
        free(fdsCopy);
    }

    pid_t childPid = child->process->id; // Important to not deref after pushing the thread
    sched_push_new_thread(child, thread);
    return childPid;

cleanup_argv:
    for (uint64_t i = 0; i < argc; i++)
    {
        free(argvCopy[i]);
    }
    free((void*)argvCopy);
    return ERR;
}

thread_t* loader_thread_create(process_t* parent, void* entry, void* arg)
{
    thread_t* child = thread_new(parent);
    if (child == NULL)
    {
        return NULL;
    }

    child->frame.rip = (uint64_t)entry;
    child->frame.rsp = child->userStack.top;
    child->frame.rdi = (uint64_t)arg;
    child->frame.cs = GDT_CS_RING3;
    child->frame.ss = GDT_SS_RING3;
    child->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;
    return child;
}

SYSCALL_DEFINE(SYS_THREAD_CREATE, tid_t, void* entry, void* arg)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;
    space_t* space = &process->space;

    if (space_check_access(space, entry, sizeof(uint64_t)) == ERR)
    {
        return ERR;
    }

    // Dont check arg user space can use it however it wants

    thread_t* newThread = loader_thread_create(process, entry, arg);
    if (newThread == NULL)
    {
        return ERR;
    }

    sched_push_new_thread(newThread, thread);
    return newThread->id;
}
