#include "syscall.h"

#include "config.h"
#include "defs.h"
#include "drivers/systime/systime.h"
#include "fs/vfs.h"
#include "fs/vfs_ctx.h"
#include "gdt.h"
#include "ipc/pipe.h"
#include "mem/vmm.h"
#include "proc/thread.h"
#include "sched/loader.h"
#include "sched/sched.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// NOTE: Syscalls should always return a 64 bit value to prevent garbage from remaining in rax.

// TODO: Improve verify funcs, improve multithreading string safety. copy_to_user? copy_from_user?
static bool verify_pointer(const void* pointer, uint64_t length)
{
    if (length == 0)
    {
        return true;
    }

    if (pointer == NULL)
    {
        return false;
    }

    if ((uint64_t)pointer + length > VMM_LOWER_HALF_MAX)
    {
        return false;
    }

    return true;
}

static bool verify_buffer(const void* pointer, uint64_t length)
{
    if (length == 0)
    {
        return true;
    }

    if (!verify_pointer(pointer, length))
    {
        return false;
    }

    if (!vmm_mapped(pointer, length))
    {
        return false;
    }

    return true;
}

static bool verify_string(const char* string)
{
    if (!verify_buffer(string, sizeof(const char*)))
    {
        return false;
    }

    const char* chr = string;
    while (true)
    {
        if (!verify_buffer(chr, sizeof(char)))
        {
            return false;
        }

        if (*chr == '\0')
        {
            return true;
        }

        chr++;
    }
}

///////////////////////////////////////////////////////

NORETURN void syscall_process_exit(uint64_t status)
{
    sched_process_exit(status);
}

NORETURN void syscall_thread_exit(void)
{
    sched_thread_exit();
}

pid_t syscall_spawn(const char** argv, const spawn_fd_t* fds)
{
    uint64_t argc = 0;
    while (1)
    {
        if (argc >= CONFIG_MAX_ARG)
        {
            return ERROR(EINVAL);
        }
        else if (!verify_buffer(&argv[argc], sizeof(const char*)))
        {
            return ERROR(EFAULT);
        }
        else if (argv[argc] == NULL)
        {
            break;
        }
        else if (!verify_string(argv[argc]))
        {
            return ERROR(EFAULT);
        }

        argc++;
    }

    uint64_t fdAmount = 0;
    if (fds != NULL)
    {
        while (1)
        {
            if (fdAmount >= CONFIG_MAX_FD)
            {
                return ERROR(EINVAL);
            }
            else if (!verify_buffer(&fds[fdAmount], sizeof(fd_t)))
            {
                return ERROR(EFAULT);
            }
            else if (fds[fdAmount].child == FD_NONE || fds[fdAmount].parent == FD_NONE)
            {
                break;
            }

            fdAmount++;
        }
    }

    thread_t* thread = loader_spawn(argv, PRIORITY_MIN);
    if (thread == NULL)
    {
        return ERR;
    }

    vfs_ctx_t* parentVfsCtx = &sched_process()->vfsCtx;
    vfs_ctx_t* childVfsCtx = &thread->process->vfsCtx;

    for (uint64_t i = 0; i < fdAmount; i++)
    {
        file_t* file = vfx_ctx_file(parentVfsCtx, fds[i].parent);
        if (file == NULL)
        {
            thread_free(thread);
            return ERROR(EBADF);
        }
        FILE_DEFER(file);

        if (vfs_ctx_openas(childVfsCtx, fds[i].child, file) == ERR)
        {
            thread_free(thread);
            return ERROR(EBADF);
        }
    }

    sched_push(thread);
    return thread->process->id;
}

uint64_t syscall_sleep(clock_t nanoseconds)
{
    return sched_sleep(nanoseconds);
}

errno_t syscall_last_error(void)
{
    return sched_thread()->error;
}

pid_t syscall_getpid(void)
{
    return sched_process()->id;
}

tid_t syscall_gettid(void)
{
    return sched_thread()->id;
}

clock_t syscall_uptime(void)
{
    return systime_uptime();
}

time_t syscall_unix_epoch(time_t* timePtr)
{
    time_t epoch = systime_unix_epoch();
    if (timePtr != NULL)
    {
        if (!verify_pointer(timePtr, sizeof(time_t)))
        {
            return ERROR(EFAULT);
        }

        *timePtr = epoch;
    }

    return epoch;
}

fd_t syscall_open(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_open(path);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_ctx_open(&sched_process()->vfsCtx, file);
}

uint64_t syscall_open2(const char* path, fd_t fds[2])
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    if (!verify_buffer(fds, sizeof(fd_t) * 2))
    {
        return ERROR(EFAULT);
    }

    file_t* files[2];
    if (vfs_open2(path, files) == ERR)
    {
        return ERR;
    }
    FILE_DEFER(files[0]);
    FILE_DEFER(files[1]);

    fds[0] = vfs_ctx_open(&sched_process()->vfsCtx, files[0]);
    if (fds[0] == ERR)
    {
        return ERR;
    }
    fds[1] = vfs_ctx_open(&sched_process()->vfsCtx, files[1]);
    if (fds[1] == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t syscall_close(fd_t fd)
{
    return vfs_ctx_close(&sched_process()->vfsCtx, fd);
}

uint64_t syscall_read(fd_t fd, void* buffer, uint64_t count)
{
    if (!verify_buffer(buffer, count))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfx_ctx_file(&sched_process()->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_read(file, buffer, count);
}

uint64_t syscall_write(fd_t fd, const void* buffer, uint64_t count)
{
    if (!verify_buffer(buffer, count))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfx_ctx_file(&sched_process()->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_write(file, buffer, count);
}

uint64_t syscall_seek(fd_t fd, int64_t offset, seek_origin_t origin)
{
    file_t* file = vfx_ctx_file(&sched_process()->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_seek(file, offset, origin);
}

uint64_t syscall_ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    if (argp != NULL && !verify_buffer(argp, size))
    {
        return ERROR(EFAULT);
    }

    if (argp == NULL && size != 0)
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfx_ctx_file(&sched_process()->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_ioctl(file, request, argp, size);
}

uint64_t syscall_chdir(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    return vfs_chdir(path);
}

uint64_t syscall_poll(pollfd_t* fds, uint64_t amount, clock_t timeout)
{
    if (amount == 0 || amount > CONFIG_MAX_FD)
    {
        return ERROR(EINVAL);
    }

    if (!verify_buffer(fds, sizeof(pollfd_t) * amount))
    {
        return ERROR(EFAULT);
    }

    poll_file_t files[CONFIG_MAX_FD];
    for (uint64_t i = 0; i < amount; i++)
    {
        files[i].file = vfx_ctx_file(&sched_process()->vfsCtx, fds[i].fd);
        if (files[i].file == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                file_deref(files[j].file);
            }
            return ERR;
        }

        files[i].requested = fds[i].requested;
        files[i].occurred = 0;
    }

    uint64_t result = vfs_poll(files, amount, timeout);

    for (uint64_t i = 0; i < amount; i++)
    {
        fds[i].occurred = files[i].occurred;
        file_deref(files[i].file);
    }

    return result;
}

uint64_t syscall_stat(const char* path, stat_t* buffer)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    if (!verify_buffer(buffer, sizeof(stat_t)))
    {
        return ERROR(EFAULT);
    }

    return vfs_stat(path, buffer);
}

void* syscall_mmap(fd_t fd, void* address, uint64_t length, prot_t prot)
{
    file_t* file = vfx_ctx_file(&sched_process()->vfsCtx, fd);
    if (file == NULL)
    {
        return NULL;
    }
    FILE_DEFER(file);

    return vfs_mmap(file, address, length, prot);
}

uint64_t syscall_munmap(void* address, uint64_t length)
{
    if (!verify_pointer(address, length))
    {
        return ERROR(EFAULT);
    }

    return vmm_unmap(address, length);
}

uint64_t syscall_mprotect(void* address, uint64_t length, prot_t prot)
{
    if (!verify_pointer(address, length))
    {
        return ERROR(EFAULT);
    }

    return vmm_protect(address, length, prot);
}

uint64_t syscall_readdir(fd_t fd, stat_t* infos, uint64_t amount)
{
    if (!verify_buffer(infos, amount * sizeof(stat_t)))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfx_ctx_file(&sched_process()->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_readdir(file, infos, amount);
}

tid_t syscall_thread_create(void* entry, void* arg)
{
    if (!verify_buffer(entry, sizeof(uint64_t)))
    {
        return ERROR(EFAULT);
    }

    thread_t* thread = loader_thread_create(sched_thread(), PRIORITY_MIN, entry, arg);

    if (thread == NULL)
    {
        return ERR;
    }

    sched_push(thread);
    return thread->id;
}

uint64_t syscall_yield(void)
{
    sched_yield();
    return 0;
}

fd_t syscall_dup(fd_t oldFd)
{
    return vfs_ctx_dup(&sched_process()->vfsCtx, oldFd);
}

fd_t syscall_dup2(fd_t oldFd, fd_t newFd)
{
    return vfs_ctx_dup2(&sched_process()->vfsCtx, oldFd, newFd);
}

uint64_t syscall_futex(atomic_uint64* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    return futex_do(addr, val, op, timeout);
}

uint64_t syscall_rename(const char* oldpath, const char* newpath)
{
    if (!verify_string(oldpath) || !verify_string(newpath))
    {
        return ERROR(EFAULT);
    }

    return vfs_rename(oldpath, newpath);
}

uint64_t syscall_remove(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    return vfs_remove(path);
}

///////////////////////////////////////////////////////

void syscall_handler_end(void)
{
    thread_t* thread = sched_thread();

    if (thread_dead(thread))
    {
        sched_thread_exit();
    }

    sched_invoke();
}

void* syscallTable[] = {
    syscall_process_exit,
    syscall_thread_exit,
    syscall_spawn,
    syscall_sleep,
    syscall_last_error,
    syscall_getpid,
    syscall_gettid,
    syscall_uptime,
    syscall_unix_epoch,
    syscall_open,
    syscall_open2,
    syscall_close,
    syscall_read,
    syscall_write,
    syscall_seek,
    syscall_ioctl,
    syscall_chdir,
    syscall_poll,
    syscall_stat,
    syscall_mmap,
    syscall_munmap,
    syscall_mprotect,
    syscall_readdir,
    syscall_thread_create,
    syscall_yield,
    syscall_dup,
    syscall_dup2,
    syscall_futex,
    syscall_rename,
    syscall_remove,
};
