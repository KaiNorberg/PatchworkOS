#include "const.h"

#include "log.h"
#include "sysfs.h"
#include "vfs.h"
#include "vmm.h"

#include <stdlib.h>
#include <string.h>

static void* const_one_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    address = vmm_alloc(address, length, prot);
    if (address == NULL)
    {
        return NULL;
    }

    memset(address, UINT32_MAX, length);
    return address;
}

static void* const_zero_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    address = vmm_alloc(address, length, prot);
    if (address == NULL)
    {
        return NULL;
    }

    memset(address, 0, length);
    return address;
}

static file_ops_t constOneOps = {
    .mmap = const_one_mmap,
};

static file_ops_t constZeroOps = {
    .mmap = const_zero_mmap,
};

void const_init(void)
{
    sysfs_expose("/const", "one", &constOneOps);
    sysfs_expose("/const", "zero", &constZeroOps);
}
