#include "const.h"

#include "log.h"
#include "sysfs.h"
#include "vmm.h"

#include <stdlib.h>
#include <string.h>

static resource_t one;
static resource_t zero;

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

static uint64_t const_one_open(resource_t* resource, file_t* file)
{
    file->ops.mmap = const_one_mmap;
    return 0;
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

static uint64_t const_zero_open(resource_t* resource, file_t* file)
{
    file->ops.mmap = const_zero_mmap;
    return 0;
}

void const_init(void)
{
    resource_init(&one, "one", const_one_open, NULL);
    sysfs_expose(&one, "/const");

    resource_init(&zero, "zero", const_zero_open, NULL);
    sysfs_expose(&zero, "/const");
}
