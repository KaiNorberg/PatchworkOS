#include "const.h"

#include "log.h"
#include "sysfs.h"
#include "vfs.h"
#include "vmm.h"

#include <stdlib.h>
#include <string.h>

static uint64_t const_one_read(file_t* file, void* buffer, uint64_t count)
{
    memset(buffer, -1, count);
    return 0;
}

static void* const_one_mmap(file_t* file, void* addr, uint64_t length, prot_t prot)
{
    addr = vmm_alloc(addr, length, prot);
    if (addr == NULL)
    {
        return NULL;
    }

    memset(addr, -1, length);
    return addr;
}

SYSFS_STANDARD_OPS_DEFINE(oneOps,
    (file_ops_t){
        .read = const_one_read,
        .mmap = const_one_mmap,
    });

static uint64_t const_zero_read(file_t* file, void* buffer, uint64_t count)
{
    memset(buffer, 0, count);
    return 0;
}

static void* const_zero_mmap(file_t* file, void* addr, uint64_t length, prot_t prot)
{
    addr = vmm_alloc(addr, length, prot);
    if (addr == NULL)
    {
        return NULL;
    }

    memset(addr, 0, length);
    return addr;
}

SYSFS_STANDARD_OPS_DEFINE(zeroOps,
    (file_ops_t){
        .read = const_zero_read,
        .mmap = const_zero_mmap,
    });

static uint64_t const_null_read(file_t* file, void* buffer, uint64_t count)
{
    return 0;
}

static uint64_t const_null_write(file_t* file, const void* buffer, uint64_t count)
{
    return 0;
}

SYSFS_STANDARD_OPS_DEFINE(nullOps,
    (file_ops_t){
        .read = const_null_read,
        .write = const_null_write,
    });

void const_init(void)
{
    ASSERT_PANIC(sysobj_new("/", "one", &oneOps, NULL) != NULL);
    ASSERT_PANIC(sysobj_new("/", "zero", &zeroOps, NULL) != NULL);
    ASSERT_PANIC(sysobj_new("/", "null", &nullOps, NULL) != NULL);
}
