#include "const.h"

#include "utils/log.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "mem/vmm.h"

#include <stdlib.h>
#include <string.h>

static sysobj_t oneObj;
static sysobj_t zeroObj;
static sysobj_t nullObj;

static uint64_t const_one_read(file_t* file, void* buffer, uint64_t count)
{
    memset(buffer, -1, count);
    return count;
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

SYSFS_STANDARD_OPS_DEFINE(oneOps, PATH_NONE,
    (file_ops_t){
        .read = const_one_read,
        .mmap = const_one_mmap,
    });

static uint64_t const_zero_read(file_t* file, void* buffer, uint64_t count)
{
    memset(buffer, 0, count);
    return count;
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

SYSFS_STANDARD_OPS_DEFINE(zeroOps, PATH_NONE,
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
    return count;
}

SYSFS_STANDARD_OPS_DEFINE(nullOps, PATH_NONE,
    (file_ops_t){
        .read = const_null_read,
        .write = const_null_write,
    });

void const_init(void)
{
    ASSERT_PANIC(sysobj_init_path(&oneObj, "/", "one", &oneOps, NULL) != ERR);
    ASSERT_PANIC(sysobj_init_path(&zeroObj, "/", "zero", &zeroOps, NULL) != ERR);
    ASSERT_PANIC(sysobj_init_path(&nullObj, "/", "null", &nullOps, NULL) != ERR);
}
