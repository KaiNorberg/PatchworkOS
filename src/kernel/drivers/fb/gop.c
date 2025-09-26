#include "gop.h"

#include "fb.h"
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

static void* gop_mmap(fb_t* fb, void* addr, uint64_t length, prot_t prot)
{
    (void)fb; // Unused

    process_t* process = sched_process();

    length = MIN(gop.height * gop.stride * sizeof(uint32_t), length);
    addr = vmm_map(&process->space, addr, gop.physAddr, length, prot, NULL, NULL);
    if (addr == NULL)
    {
        return NULL;
    }
    return addr;
}

static fb_t fb = {
    .info = {0}, // Set in gop_init
    .mmap = gop_mmap,
};

void gop_init(boot_gop_t* in)
{
    fb.info.width = in->width;
    fb.info.height = in->height;
    fb.info.stride = in->stride;
    fb.info.format = FB_ARGB32;
    gop = *in;

    if (fb_expose(&fb) == ERR)
    {
        panic(NULL, "Failed to expose gop framebuffer");
    }
}
