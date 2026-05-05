#include <libc/status.h>

const char* _sourceStrings[] = {
    [STATUS_SRC_NONE] = "none",
    [STATUS_SRC_IO] = "io",
    [STATUS_SRC_MEM] = "mem",
    [STATUS_SRC_MMU] = "mmu",
    [STATUS_SRC_SIMD] = "simd",
    [STATUS_SRC_SCHED] = "sched",
    [STATUS_SRC_INT] = "int",
    [STATUS_SRC_SYNC] = "sync",
    [STATUS_SRC_DRIVER] = "driver",
    [STATUS_SRC_FS] = "fs",
    [STATUS_SRC_VFS] = "vfs",
    [STATUS_SRC_IPC] = "ipc",
    [STATUS_SRC_LIBSTD] = "libc",
    [STATUS_SRC_USER] = "user",
    [STATUS_SRC_PROC] = "proc",
    [STATUS_SRC_MODULE] = "module",
    [STATUS_SRC_PORT] = "port",
    [STATUS_SRC_SYSCALL] = "syscall",
    [STATUS_SRC_ACPI] = "acpi",
    [STATUS_SRC_TEST] = "test",
    [STATUS_SRC_PROTO] = "proto",
    [STATUS_SRC_DYNLINK] = "dynlink",
};

const char* status_src_str(status_src_t src)
{
    if (src < STATUS_SRC_NONE || src >= STATUS_SRC_MAX)
    {
        return "unknown";
    }
    return _sourceStrings[src];
}