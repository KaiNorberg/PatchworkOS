#include <sys/status.h>

const char* _sourceStrings[] = {
    [ST_SRC_NONE] = "none",
    [ST_SRC_IO] = "io",
    [ST_SRC_MEM] = "mem",
    [ST_SRC_MMU] = "mmu",
    [ST_SRC_SIMD] = "simd",
    [ST_SRC_SCHED] = "sched",
    [ST_SRC_INT] = "int",
    [ST_SRC_SYNC] = "sync",
    [ST_SRC_DRIVER] = "driver",
    [ST_SRC_FS] = "fs",
    [ST_SRC_VFS] = "vfs",
    [ST_SRC_IPC] = "ipc",
    [ST_SRC_LIBSTD] = "libstd",
    [ST_SRC_USER] = "user",
    [ST_SRC_PROC] = "proc",
    [ST_SRC_MODULE] = "module",
    [ST_SRC_PORT] = "port",
    [ST_SRC_SYSCALL] = "syscall",
};

const char* srctostr(st_src_t src)
{
    if (src < ST_SRC_NONE || src > ST_SRC_USER)
    {
        return "unknown";
    }
    return _sourceStrings[src];
}