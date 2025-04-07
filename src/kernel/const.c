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

static file_ops_t oneOps = {
    .mmap = const_one_mmap,
};

static file_ops_t zeroOps = {
    .mmap = const_zero_mmap,
};

static file_t* const_one_open(volume_t* volume, resource_t* resource)
{
    file_t* file = file_new(volume);
    if (file == NULL)
    {
        return NULL;
    }
    file->ops = &oneOps;

    return file;
}

static file_t* const_zero_open(volume_t* volume, resource_t* resource)
{
    file_t* file = file_new(volume);
    if (file == NULL)
    {
        return NULL;
    }
    file->ops = &zeroOps;

    return file;
}

static resource_ops_t oneResOps = {
    .open = const_one_open,
};

static resource_ops_t zeroResOps = {
    .open = const_zero_open,
};

void const_init(void)
{
    sysfs_expose("/", "one", &oneResOps, NULL);
    sysfs_expose("/", "zero", &zeroResOps, NULL);
}
