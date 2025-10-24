#include "gop.h"

#include "drivers/abstractions/fb.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "proc/process.h"
#include "sched/sched.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/fb.h>
#include <sys/math.h>

static boot_gop_t gop;
static fb_t* fb;

static void* gop_mmap(fb_t* fb, void* addr, uint64_t length, pml_flags_t flags)
{
    (void)fb; // Unused

    process_t* process = sched_process();

    length = MIN(gop.height * gop.stride * sizeof(uint32_t), length);
    addr = vmm_map(&process->space, addr, gop.physAddr, length, flags, NULL, NULL);
    if (addr == NULL)
    {
        return NULL;
    }
    return addr;
}

static fb_info_t info;

void gop_init(const boot_gop_t* in)
{
    gop = *in;
    info.width = in->width;
    info.height = in->height;
    info.stride = in->stride;
    info.format = FB_ARGB32;
    strncpy(info.name, "GOP Framebuffer", MAX_NAME - 1);
    info.name[MAX_NAME - 1] = '\0';

    fb = fb_new(&info, gop_mmap);
    if (fb == NULL)
    {
        panic(NULL, "Failed to create GOP framebuffer");
    }
}
