#include "const.h"

#include "sysfs.h"
#include "heap.h"
#include "tty.h"
#include "vmm.h"

#include <string.h>

static Resource one;
static Resource zero;

void* const_one_mmap(File* file, void* address, uint64_t length, uint8_t prot)
{
    if (vmm_allocate(address, length, prot) == NULL)
    {
        return NULL;
    }

    memset(address, UINT32_MAX, length);
    return 0;
}

void* const_zero_mmap(File* file, void* address, uint64_t length, uint8_t prot)
{
    if (vmm_allocate(address, length, prot) == NULL)
    {
        return NULL;
    }

    memset(address, 0, length);
    return 0;
}

void const_init(void)
{    
    tty_start_message("Constants initializing");

    resource_init(&one, "one");
    one.methods.mmap = const_one_mmap;
    sysfs_expose(&one, "/const");

    resource_init(&zero, "zero");
    zero.methods.mmap = const_zero_mmap;
    sysfs_expose(&zero, "/const");

    tty_end_message(TTY_MESSAGE_OK);
}