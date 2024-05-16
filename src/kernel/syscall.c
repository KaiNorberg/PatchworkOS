#include "syscall.h"

#include <string.h>
#include <errno.h>

#include "defs.h"
#include "tty.h"
#include "heap.h"
#include "vmm.h"
#include "time.h"
#include "utils.h"
#include "smp.h"
#include "debug.h"
#include "sched.h"
#include "loader.h"

//NOTE: Syscalls should always return a 64 bit value to prevent garbage from remaining in rax.

//TODO: Improve verify funcs, improve multithreading string safety.
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

    /*if (!vmm_mapped(pointer, length))
    {
        return false;
    }*/

    return true;
}

static bool verify_string(const char* string)
{
    return verify_buffer(string, 0) && verify_buffer(string, strlen(string));
}

///////////////////////////////////////////////////////

NORETURN void syscall_process_exit(uint64_t status)
{
    sched_process_exit(status);
}

NORETURN void syscall_thread_exit()
{
    sched_thread_exit();
}

uint64_t syscall_spawn(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    return sched_spawn(path);
}

uint64_t syscall_sleep(uint64_t nanoseconds)
{
    sched_sleep(nanoseconds);
    return 0;
}

uint64_t syscall_error(void)
{
    return sched_thread()->error;
}

uint64_t syscall_pid(void)
{
    return sched_process()->id;
}

uint64_t syscall_tid(void)
{
    return sched_thread()->id;
}

uint64_t syscall_open(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    File* file = vfs_open(path);
    if (file == NULL)
    {
        return ERR;
    }

    return vfs_context_open(file);
}

uint64_t syscall_close(uint64_t fd)
{
    return vfs_context_close(fd);
}

uint64_t syscall_read(uint64_t fd, void* buffer, uint64_t count)
{
    if (!verify_buffer(buffer, count))
    {
        return ERROR(EFAULT);
    }

    File* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }

    return FILE_CALL_METHOD(file, read, buffer, count);
}

uint64_t syscall_write(uint64_t fd, const void* buffer, uint64_t count)
{
    if (!verify_buffer(buffer, count))
    {
        return ERROR(EFAULT);
    }

    File* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }

    return FILE_CALL_METHOD(file, write, buffer, count);
}

uint64_t syscall_seek(uint64_t fd, int64_t offset, uint8_t origin)
{
    File* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }

    return FILE_CALL_METHOD(file, seek, offset, origin);
}

uint64_t syscall_ioctl(uint64_t fd, uint64_t request, void* buffer, uint64_t length)
{
    if (!verify_buffer(buffer, length))
    {
        return ERROR(EFAULT);
    }

    File* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }

    return FILE_CALL_METHOD(file, ioctl, request, buffer, length);
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

void* syscall_mmap(uint64_t fd, void* address, uint64_t length, uint16_t flags)
{
    File* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return NULL;
    }

    return FILE_CALL_METHOD_PTR(file, mmap, address, length, flags);
}

uint64_t syscall_munmap(void* address, uint64_t length)
{
    if (!verify_pointer(address, length))
    {
        return ERROR(EFAULT);
    }

    return vmm_unmap(address, length);
}

uint64_t syscall_mprotect(void* address, uint64_t length, uint16_t flags)
{
    if (!verify_pointer(address, length))
    {
        return ERROR(EFAULT);
    }
    
    return vmm_protect(address, length, flags);
}

///////////////////////////////////////////////////////

void syscall_handler_end(void)
{
    if (sched_process()->killed)
    {
        sched_thread_exit();
    }

    sched_yield();
}

void* syscallTable[] =
{
    syscall_process_exit,
    syscall_thread_exit,
    syscall_spawn,
    syscall_sleep,
    syscall_error,
    syscall_pid,
    syscall_tid,
    syscall_open,
    syscall_close,
    syscall_read,
    syscall_write,
    syscall_seek,
    syscall_ioctl,
    syscall_realpath,
    syscall_chdir,
    syscall_mmap,
    syscall_munmap,
    syscall_mprotect
};