#include <kernel/drivers/gop.h>

#include <kernel/abstract/fb.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/fb.h>
#include <sys/math.h>

static boot_gop_t gop;
static fb_t* fb;

static void* gop_mmap(fb_t* fb, void* addr, uint64_t length, uint64_t* offset, pml_flags_t flags)
{
    (void)fb; // Unused

    process_t* process = sched_process();

    uintptr_t physAddr = (uint64_t)gop.physAddr + *offset;
    uintptr_t endAddr = physAddr + length;
    if (endAddr > (uint64_t)gop.physAddr + (gop.stride * gop.height * sizeof(uint32_t)))
    {
        errno = EINVAL;
        return NULL;
    }

    addr = vmm_map(&process->space, addr, (void*)physAddr, length, flags, NULL, NULL);
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
