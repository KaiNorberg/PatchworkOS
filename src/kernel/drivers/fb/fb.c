#include "fb.h"

#include "defs.h"
#include "fs/sysfs.h"
#include "sched/thread.h"
#include "utils/log.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/fb.h>

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static void* fb_mmap(file_t* file, void* addr, uint64_t length, prot_t prot)
{
    log_disable_screen();

    fb_t* fb = file->private;
    return fb->mmap(fb, addr, length, prot);
}

static uint64_t fb_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    fb_t* fb = file->private;
    switch (request)
    {
    case IOCTL_FB_INFO:
    {
        if (size < sizeof(fb_info_t))
        {
            return ERROR(EINVAL);
        }

        memcpy(argp, &fb->info, sizeof(fb_info_t));
    }
    break;
    default:
    {
        return ERROR(EINVAL);
    }
    }

    return 0;
}

SYSFS_STANDARD_OPS_DEFINE(fbOps, PATH_NONE,
    (file_ops_t){
        .mmap = fb_mmap,
        .ioctl = fb_ioctl,
    });

uint64_t fb_expose(fb_t* fb)
{
    char name[MAX_NAME];
    sprintf(name, "fb%d", atomic_load(&newId));
    assert(sysobj_init_path(&fb->sysobj, "/", name, &fbOps, fb) != ERR);
    return 0;
}
