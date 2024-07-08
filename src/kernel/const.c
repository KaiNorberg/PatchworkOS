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

static file_ops_t constOneOps = {
    .mmap = const_one_mmap,
};

static file_ops_t constZeroOps = {
    .mmap = const_zero_mmap,
};

void const_init(void)
{
    sysfs_expose("/const", "one", &constOneOps, NULL, NULL);
    sysfs_expose("/const", "zero", &constZeroOps, NULL, NULL);
}
