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

static uint64_t fb_name_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    fb_t* fb = file->inode->private;
    uint64_t nameLen = strnlen_s(fb->name, MAX_NAME);
    if (*offset >= nameLen)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, fb->name, nameLen);
}

static file_ops_t nameOps = {
    .read = fb_name_read,
};

static void fb_dir_cleanup(inode_t* inode)
{
    fb_t* fb = inode->private;
    heap_free(fb);
}

static inode_ops_t dirInodeOps = {
    .cleanup = fb_dir_cleanup,
};

fb_t* fb_new(const fb_info_t* info, fb_mmap_t mmap, const char* name)
{
    if (info == NULL || mmap == NULL || name == NULL)
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

    if (sprintf(fb->id, "fb%d", atomic_fetch_add(&newId, 1)) < 0)
    {
        return NULL;
    }
    strncpy(fb->name, name, MAX_NAME - 1);
    fb->name[MAX_NAME - 1] = '\0';
    memcpy(&fb->info, info, sizeof(fb_info_t));
    fb->mmap = mmap;
    fb->dir = sysfs_dir_new(fbDir, fb->id, &dirInodeOps, fb);
    if (fb->dir == NULL)
    {
        return NULL;
    }

    dentry_t* bufferFile = sysfs_file_new(fb->dir, "buffer", NULL, &bufferOps, fb);
    DEREF(bufferFile);
    dentry_t* infoFile = sysfs_file_new(fb->dir, "info", NULL, &infoOps, fb);
    DEREF(infoFile);
    dentry_t* nameFile = sysfs_file_new(fb->dir, "name", NULL, &nameOps, fb);
    DEREF(nameFile);
    if (bufferFile == NULL || infoFile == NULL || nameFile == NULL)
    {
        DEREF(fb->dir);
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
    // fb is freed in fb_dir_cleanup
}
