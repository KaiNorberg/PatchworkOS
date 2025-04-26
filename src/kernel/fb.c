#include "fb.h"

#include "sysfs.h"
#include "defs.h"
#include "sched.h"

#include <stdatomic.h>
#include <sys/atomint.h>

static atomic_uint64 newId = ATOMIC_VAR_INIT(0);

static uint64_t fb_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    return ERROR(EIMPL);
}

uint64_t fb_expose(fb_t* fb)
{
    return ERROR(EIMPL);
}
