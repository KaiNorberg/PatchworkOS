#include "syscall.h"

#include "config.h"
#include "defs.h"
#include "gdt.h"
#include "loader.h"
#include "pipe.h"
#include "sched.h"
#include "systime.h"
#include "thread.h"
#include "vfs.h"
#include "vfs_context.h"
#include "vmm.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// NOTE: Syscalls should always return a 64 bit value to prevent garbage from remaining in rax.

// TODO: Improve verify funcs, improve multithreading string safety. copy_to_user? copy_from_user?
static bool verify_pointer(const void* pointer, uint64_t length)
{
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

    vfs_context_t* parentVfsContext = &sched_process()->vfsContext;
    vfs_context_t* childVfsContext = &thread->process->vfsContext;

    for (uint64_t i = 0; i < fdAmount; i++)
    {
        file_t* file = vfs_context_get(parentVfsContext, fds[i].parent);
        if (file == NULL)
        {
            thread_free(thread);
            return ERROR(EBADF);
        }
        FILE_DEFER(file);

        if (vfs_context_openat(childVfsContext, fds[i].child, file) == ERR)
        {
            thread_free(thread);
            return ERROR(EBADF);
        }
    }

    sched_push(thread);
    return thread->process->id;
}

uint64_t syscall_sleep(nsec_t nanoseconds)
{
    return sched_sleep(nanoseconds);
}

errno_t syscall_error(void)
{
    return sched_thread()->error;
}

pid_t syscall_pid(void)
{
    return sched_process()->id;
}

tid_t syscall_tid(void)
{
    return sched_thread()->id;
}

nsec_t syscall_uptime(void)
{
    return systime_uptime();
}

time_t syscall_time(time_t* timePtr)
{
    time_t epoch = systime_time();
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

    return vfs_context_open(&sched_process()->vfsContext, file);
}

uint64_t syscall_close(fd_t fd)
{
    return vfs_context_close(&sched_process()->vfsContext, fd);
}

uint64_t syscall_read(fd_t fd, void* buffer, uint64_t count)
{
    if (!verify_buffer(buffer, count))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_context_get(&sched_process()->vfsContext, fd);
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

    file_t* file = vfs_context_get(&sched_process()->vfsContext, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_write(file, buffer, count);
}

uint64_t syscall_seek(fd_t fd, int64_t offset, seek_origin_t origin)
{
    file_t* file = vfs_context_get(&sched_process()->vfsContext, fd);
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

    file_t* file = vfs_context_get(&sched_process()->vfsContext, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_ioctl(file, request, argp, size);
}

uint64_t syscall_realpath(char* out, const char* path)
{
    if (!verify_pointer(out, 0) || !verify_string(path))
    {
        return ERROR(EFAULT);
    }

    return vfs_realpath(out, path);
}

uint64_t syscall_chdir(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    return vfs_chdir(path);
}

uint64_t syscall_poll(pollfd_t* fds, uint64_t amount, nsec_t timeout)
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
        files[i].file = vfs_context_get(&sched_process()->vfsContext, fds[i].fd);
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
    file_t* file = vfs_context_get(&sched_process()->vfsContext, fd);
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

uint64_t syscall_flush(fd_t fd, const pixel_t* buffer, uint64_t size, const rect_t* rect)
{
    if (!verify_buffer(buffer, size))
    {
        return ERROR(EFAULT);
    }

    if (!verify_buffer(rect, sizeof(rect_t)))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_context_get(&sched_process()->vfsContext, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_flush(file, buffer, size, rect);
}

uint64_t syscall_listdir(const char* path, dir_entry_t* entries, uint64_t amount)
{
    if (entries != NULL && !verify_buffer(entries, sizeof(dir_entry_t) * amount))
    {
        return ERROR(EFAULT);
    }

    return vfs_listdir(path, entries, amount);
}

uint64_t syscall_pipe(pipefd_t* pipefd)
{
    if (!verify_buffer(pipefd, sizeof(pipefd_t)))
    {
        return ERROR(EFAULT);
    }

    pipe_file_t pipe;
    if (pipe_init(&pipe) == ERR)
    {
        return ERR;
    }
    FILE_DEFER(pipe.read);
    FILE_DEFER(pipe.write);

    vfs_context_t* vfsContext = &sched_process()->vfsContext;

    pipefd->read = vfs_context_open(vfsContext, pipe.read);
    if (pipefd->read == ERR)
    {
        return ERR;
    }

    pipefd->write = vfs_context_open(vfsContext, pipe.write);
    if (pipefd->write == ERR)
    {
        vfs_context_close(vfsContext, pipefd->read);
        return ERR;
    }

    return 0;
}

tid_t syscall_split(void* entry, uint64_t argc, ...)
{
    if (argc > LOADER_SPLIT_MAX_ARGS)
    {
        return ERROR(EINVAL);
    }

    if (!verify_buffer(entry, sizeof(uint64_t)))
    {
        return ERROR(EFAULT);
    }

    va_list args;
    va_start(args, argc);
    thread_t* thread = loader_split(sched_thread(), entry, PRIORITY_MIN, argc, args);
    va_end(args);

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

///////////////////////////////////////////////////////

void syscall_handler_end(void)
{
    if (atomic_load(&sched_process()->dead))
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
    syscall_error,
    syscall_pid,
    syscall_tid,
    syscall_uptime,
    syscall_time,
    syscall_open,
    syscall_close,
    syscall_read,
    syscall_write,
    syscall_seek,
    syscall_ioctl,
    syscall_realpath,
    syscall_chdir,
    syscall_poll,
    syscall_stat,
    syscall_mmap,
    syscall_munmap,
    syscall_mprotect,
    syscall_flush,
    syscall_listdir,
    syscall_pipe,
    syscall_split,
    syscall_yield,
};
