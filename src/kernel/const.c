#include "const.h"

#include "splash.h"
#include "sysfs.h"
#include "vmm.h"

#include <stdlib.h>
#include <string.h>

static Resource one;
static Resource zero;

static void* const_one_mmap(File* file, void* address, uint64_t length, prot_t prot)
{
    address = vmm_alloc(address, length, prot);
    if (address == NULL)
    {
        return NULL;
    }

    memset(address, UINT32_MAX, length);
    return address;
}

static uint64_t const_one_open(Resource* resource, File* file)
{
    file->methods.mmap = const_one_mmap;
    return 0;
}

static void* const_zero_mmap(File* file, void* address, uint64_t length, prot_t prot)
{
    address = vmm_alloc(address, length, prot);
    if (address == NULL)
    {
        return NULL;
    }

    memset(address, 0, length);
    return address;
}

static uint64_t const_zero_open(Resource* resource, File* file)
{
    file->methods.mmap = const_zero_mmap;
    return 0;
}

void const_init(void)
{
    resource_init(&one, "one", const_one_open, NULL);
    sysfs_expose(&one, "/const");

    resource_init(&zero, "zero", const_zero_open, NULL);
    sysfs_expose(&zero, "/const");
}
