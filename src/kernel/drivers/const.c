#include "const.h"

#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "proc/process.h"
#include "sched/sched.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static dentry_t* oneFile;
static dentry_t* zeroFile;
static dentry_t* nullFile;

static uint64_t const_one_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    memset(buffer, -1, count);
    *offset += count;
    return count;
}

static void* const_one_mmap(file_t* file, void* addr, uint64_t length, uint64_t* offset, pml_flags_t flags)
{
    (void)file;   // Unused
    (void)offset; // Unused

    addr = vmm_alloc(&sched_process()->space, addr, length, flags, VMM_ALLOC_NONE);
    if (addr == NULL)
    {
        return NULL;
    }

    memset(addr, -1, length);
    return addr;
}

static file_ops_t oneOps = {
    .read = const_one_read,
    .mmap = const_one_mmap,
};

static uint64_t const_zero_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    memset(buffer, 0, count);
    *offset += count;
    return count;
}

static void* const_zero_mmap(file_t* file, void* addr, uint64_t length, uint64_t* offset, pml_flags_t flags)
{
    (void)file;   // Unused
    (void)offset; // Unused

    addr = vmm_alloc(&sched_process()->space, addr, length, flags, VMM_ALLOC_NONE);
    if (addr == NULL)
    {
        return NULL;
    }

    memset(addr, 0, length);
    return addr;
}

static file_ops_t zeroOps = {
    .read = const_zero_read,
    .mmap = const_zero_mmap,
};

static uint64_t const_null_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file;   // Unused
    (void)buffer; // Unused

    *offset += count;
    return 0;
}

static uint64_t const_null_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file;   // Unused
    (void)buffer; // Unused

    *offset += count;
    return count;
}

static file_ops_t nullOps = {
    .read = const_null_read,
    .write = const_null_write,
};

void const_init(void)
{
    oneFile = sysfs_file_new(NULL, "one", NULL, &oneOps, NULL);
    if (oneFile == NULL)
    {
        panic(NULL, "Failed to init one file");
    }

    zeroFile = sysfs_file_new(NULL, "zero", NULL, &zeroOps, NULL);
    if (zeroFile == NULL)
    {
        DEREF(oneFile);
        panic(NULL, "Failed to init zero file");
    }

    nullFile = sysfs_file_new(NULL, "null", NULL, &nullOps, NULL);
    if (nullFile == NULL)
    {
        DEREF(oneFile);
        DEREF(zeroFile);
        panic(NULL, "Failed to init null file");
    }
}
