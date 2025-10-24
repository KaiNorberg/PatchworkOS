#include "fb.h"

#include "fs/file.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "mem/heap.h"
#include "sched/thread.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/fb.h>

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static dentry_t* fbDir = NULL;

static void* fb_buffer_mmap(file_t* file, void* addr, uint64_t length, pml_flags_t flags)
{
    log_screen_disable();

    fb_t* fb = file->inode->private;
    return fb->mmap(fb, addr, length, flags);
}

static file_ops_t bufferOps = {
    .mmap = fb_buffer_mmap,
};

static uint64_t fb_info_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    fb_t* fb = file->inode->private;
    if (*offset >= sizeof(fb_info_t))
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, &fb->info, sizeof(fb_info_t));
}

static file_ops_t infoOps = {
    .read = fb_info_read,
};

static void fb_dir_cleanup(inode_t* inode)
{
    fb_t* fb = inode->private;
    heap_free(fb);
}

static inode_ops_t dirInodeOps = {
    .cleanup = fb_dir_cleanup,
};

fb_t* fb_new(const fb_info_t* info, fb_mmap_t mmap)
{
    if (info == NULL || mmap == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (fbDir == NULL)
    {
        fbDir = sysfs_dir_new(NULL, "fb", &dirInodeOps, NULL);
        if (fbDir == NULL)
        {
            return NULL;
        }
    }

    fb_t* fb = heap_alloc(sizeof(fb_t), HEAP_NONE);
    if (fb == NULL)
    {
        return NULL;
    }
    memcpy(&fb->info, info, sizeof(fb_info_t));
    fb->mmap = mmap;

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        heap_free(fb);
        return NULL;
    }

    fb->dir = sysfs_dir_new(fbDir, id, &dirInodeOps, fb);
    if (fb->dir == NULL)
    {
        heap_free(fb);
        return NULL;
    }
    fb->bufferFile = sysfs_file_new(fb->dir, "buffer", NULL, &bufferOps, fb);
    if (fb->bufferFile == NULL)
    {
        DEREF(fb->dir); // fb will be freed in fb_dir_cleanup
        return NULL;
    }
    fb->infoFile = sysfs_file_new(fb->dir, "info", NULL, &infoOps, fb);
    if (fb->infoFile == NULL)
    {
        DEREF(fb->dir);
        DEREF(fb->bufferFile);
        return NULL;
    }

    return fb;
}

void fb_free(fb_t* fb)
{
    if (fb == NULL)
    {
        return;
    }

    DEREF(fb->dir);
    DEREF(fb->bufferFile);
    DEREF(fb->infoFile);
    // fb is freed in fb_dir_cleanup
}
