#include "const.h"

#include "log.h"
#include "sysfs.h"
#include "vfs.h"
#include "vmm.h"

#include <stdlib.h>
#include <string.h>

static void* const_one_mmap(file_t* file, void* addr, uint64_t length, prot_t prot)
{
    addr = vmm_alloc(addr, length, prot);
    if (addr == NULL)
    {
        return NULL;
    }

    memset(addr, UINT32_MAX, length);
    return addr;
}

static file_ops_t oneOps = {
    .mmap = const_one_mmap,
};

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

static file_ops_t zeroOps = {
    .mmap = const_zero_mmap,
};

static uint64_t const_null_read(file_t* file, void* buffer, uint64_t count)
{
    return 0;
}

static uint64_t const_null_write(file_t* file, const void* buffer, uint64_t count)
{
    return 0;
}

static file_ops_t nullOps = {
    .read = const_null_read,
    .write = const_null_write,
};

SYSFS_STANDARD_RESOURCE_OPS(oneResOps, &oneOps);
SYSFS_STANDARD_RESOURCE_OPS(zeroResOps, &zeroOps);
SYSFS_STANDARD_RESOURCE_OPS(nullResOps, &nullOps);

void const_init(void)
{
    ASSERT_PANIC(sysfs_expose("/", "one", &oneResOps, NULL) != NULL, "const one");
    ASSERT_PANIC(sysfs_expose("/", "zero", &zeroResOps, NULL) != NULL, "const zero");
    ASSERT_PANIC(sysfs_expose("/", "null", &nullResOps, NULL) != NULL, "const null");
}
