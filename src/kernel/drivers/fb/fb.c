#include "fb.h"

#include "fs/sysfs.h"
#include "log/log.h"
#include "sched/thread.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/fb.h>

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static void* fb_mmap(file_t* file, void* addr, uint64_t length, pml_flags_t flags)
{
    log_screen_disable();

    fb_t* fb = file->inode->private;
    return fb->mmap(fb, addr, length, flags);
}

static uint64_t fb_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    fb_t* fb = file->inode->private;
    switch (request)
    {
    case IOCTL_FB_INFO:
    {
        if (size < sizeof(fb_info_t))
        {
            errno = EINVAL;
            return ERR;
        }

        memcpy(argp, &fb->info, sizeof(fb_info_t));
    }
    break;
    default:
    {
        errno = EINVAL;
        return ERR;
    }
    }

    return 0;
}

static file_ops_t fbOps = {
    .mmap = fb_mmap,
    .ioctl = fb_ioctl,
};

uint64_t fb_expose(fb_t* fb)
{
    char name[MAX_NAME];
    sprintf(name, "fb%d", atomic_load(&newId));
    if (sysfs_file_init(&fb->file, sysfs_get_default(), name, NULL, &fbOps, fb) == ERR)
    {
        return ERR;
    }
    return 0;
}
