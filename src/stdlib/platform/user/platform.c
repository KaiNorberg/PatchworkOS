#include "../platform.h"

#include <sys/io.h>
#include <sys/proc.h>

static fd_t zeroResource;

void _PlatformInit(void)
{
    zeroResource = open("sys:/zero");
}

void* _PlatformPageAlloc(uint64_t amount)
{
    return mmap(zeroResource, NULL, amount * PAGE_SIZE, PROT_READ | PROT_WRITE);
}