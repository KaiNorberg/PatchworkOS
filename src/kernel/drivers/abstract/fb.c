#include <kernel/drivers/abstract/fb.h>

#include <kernel/fs/file.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/sched/thread.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static dentry_t* dir = NULL;

static uint64_t fb_name_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    fb_t* fb = file->inode->private;
    assert(fb != NULL);

    uint64_t length = strlen(fb->name);
    return BUFFER_READ(buffer, count, offset, fb->name, length);
}

static file_ops_t nameOps = {
    .read = fb_name_read,
};

static uint64_t fb_buffer_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    fb_t* fb = file->inode->private;
    assert(fb != NULL);

    if (fb->ops->read == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    return fb->ops->read(fb, buffer, count, offset);
}

static uint64_t fb_buffer_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    fb_t* fb = file->inode->private;
    assert(fb != NULL);

    if (fb->ops->write == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    return fb->ops->write(fb, buffer, count, offset);
}

static void* fb_buffer_mmap(file_t* file, void* addr, uint64_t length, uint64_t* offset, pml_flags_t flags)
{
    fb_t* fb = file->inode->private;
    assert(fb != NULL);

    if (fb->ops->mmap == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    return fb->ops->mmap(fb, addr, length, offset, flags);
}

static file_ops_t dataOps = {
    .read = fb_buffer_read,
    .write = fb_buffer_write,
    .mmap = fb_buffer_mmap,
};

static uint64_t fb_info_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    fb_t* fb = file->inode->private;
    assert(fb != NULL);

    if (fb->ops->info == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    fb_info_t info = {0};
    if (fb->ops->info(fb, &info) == ERR)
    {
        return ERR;
    }

    char string[256];
    int length = snprintf(string, sizeof(string), "%llu %llu %llu %s", info.width, info.height, info.pitch, info.format);
    if (length < 0)
    {
        errno = EIO;
        return ERR;
    }

    if ((uint64_t)length > count)
    {
        errno = EOVERFLOW;
        return ERR;
    }

    return BUFFER_READ(buffer, count, offset, string, length);
}

static file_ops_t infoOps = {
    .read = fb_info_read,
};

static void fb_dir_cleanup(inode_t* inode)
{
    fb_t* fb = inode->private;

    if (fb->ops->cleanup != NULL)
    {
        fb->ops->cleanup(fb);
    }

    free(fb);
}

static inode_ops_t dirInodeOps = {
    .cleanup = fb_dir_cleanup,
};

fb_t* fb_new(const char* name, const fb_ops_t* ops, void* private)
{
    if (name == NULL || ops == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (dir == NULL)
    {
        dir = devfs_dir_new(NULL, "fb", NULL, NULL);
        if (dir == NULL)
        {
            return NULL;
        }
    }

    fb_t* fb = malloc(sizeof(fb_t));
    if (fb == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    strncpy(fb->name, name, FB_MAX_NAME);
    fb->name[FB_MAX_NAME - 1] = '\0';
    fb->ops = ops;
    fb->private = private;
    fb->dir = NULL;
    list_init(&fb->files);

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        free(fb);
        return NULL;
    }

    fb->dir = devfs_dir_new(dir, id, &dirInodeOps, fb);
    if (fb->dir == NULL)
    {
        free(fb);
        return NULL;
    }

    devfs_file_desc_t files[] = {
        {
            .name = "name",
            .inodeOps = NULL,
            .fileOps = &nameOps,
            .private = fb,
        },
        {
            .name = "info",
            .inodeOps = NULL,
            .fileOps = &infoOps,
            .private = fb,
        },
        {
            .name = "data",
            .inodeOps = NULL,
            .fileOps = &dataOps,
            .private = fb,
        },
        {
            .name = NULL,
        },
    };

    if (devfs_files_new(&fb->files, fb->dir, files) == ERR)
    {
        UNREF(fb->dir);
        free(fb);
        return NULL;
    }

    LOG_INFO("new framebuffer device with name %s and id %s", fb->name, id);
    return fb;
}

void fb_free(fb_t* fb)
{
    if (fb == NULL)
    {
        return;
    }

    UNREF(fb->dir);
    devfs_files_free(&fb->files);
}
