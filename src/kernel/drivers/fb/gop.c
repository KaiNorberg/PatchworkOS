#include "gop.h"

#include "defs.h"
#include "fb.h"
#include "log/log.h"
#include "mem/vmm.h"
#include "proc/process.h"
#include "sched/sched.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/fb.h>
#include <sys/math.h>

static gop_buffer_t gop;

static void* gop_mmap(fb_t* fb, void* addr, uint64_t length, prot_t prot)
{
    process_t* process = sched_process();

    length = MIN(gop.height * gop.stride * sizeof(uint32_t), length);
    addr = vmm_map(&process->space, addr, gop.base, length, prot, NULL, NULL);
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

void gop_init(gop_buffer_t* gopBuffer)
{
    fb.info.width = gopBuffer->width;
    fb.info.height = gopBuffer->height;
    fb.info.stride = gopBuffer->stride;
    fb.info.format = FB_ARGB32;
    gop = *gopBuffer;

    assert(fb_expose(&fb) != ERR);
}
