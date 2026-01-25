#include <kernel/drivers/abstract/fb.h>

#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/sched/thread.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static dentry_t* dir = NULL;

static status_t fb_name_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    fb_t* fb = file->vnode->data;
    assert(fb != NULL);

    uint64_t length = strlen(fb->name);
    *bytesRead = BUFFER_READ(buffer, count, offset, fb->name, length);
    return OK;
}

static file_ops_t nameOps = {
    .read = fb_name_read,
};

static status_t fb_data_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    fb_t* fb = file->vnode->data;
    assert(fb != NULL);

    if (fb->read == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    return fb->read(fb, buffer, count, offset, bytesRead);
}

static status_t fb_data_write(file_t* file, const void* buffer, size_t count, size_t* offset, size_t* bytesWritten)
{
    fb_t* fb = file->vnode->data;
    assert(fb != NULL);

    if (fb->write == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    return fb->write(fb, buffer, count, offset, bytesWritten);
}

static status_t fb_data_mmap(file_t* file, void** addr, size_t length, size_t* offset, pml_flags_t flags)
{
    fb_t* fb = file->vnode->data;
    assert(fb != NULL);

    if (fb->mmap == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    return fb->mmap(fb, addr, length, offset, flags);
}

static file_ops_t dataOps = {
    .read = fb_data_read,
    .write = fb_data_write,
    .mmap = fb_data_mmap,
};

static status_t fb_info_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    fb_t* fb = file->vnode->data;
    assert(fb != NULL);

    if (fb->info == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    fb_info_t info = {0};
    status_t status = fb->info(fb, &info);
    if (IS_FAIL(status))
    {
        return status;
    }

    char string[256];
    int length = snprintf(string, sizeof(string), "%llu %llu %llu %s", info.width, info.height, info.pitch, info.format);
    assert(length > 0);

    if ((size_t)length >= sizeof(string))
    {
        return ERR(DRIVER, OVERFLOW);
    }

    *bytesRead = BUFFER_READ(buffer, count, offset, string, (size_t)length);
    return OK;
}

static file_ops_t infoOps = {
    .read = fb_info_read,
};

static void fb_dir_cleanup(vnode_t* vnode)
{
    fb_t* fb = vnode->data;

    if (fb->cleanup != NULL)
    {
        fb->cleanup(fb);
    }
}

static vnode_ops_t dirVnodeOps = {
    .cleanup = fb_dir_cleanup,
};

status_t fb_register(fb_t* fb)
{
    if (fb == NULL || fb->name == NULL || fb->info == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    if (dir == NULL)
    {
        dir = devfs_dir_new(NULL, "fb", NULL, NULL);
        if (dir == NULL)
        {
            return ERR(DRIVER, NOMEM);
        }
    }

    list_init(&fb->files);

    char id[MAX_NAME];
    snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1));

    fb->dir = devfs_dir_new(dir, id, &dirVnodeOps, fb);
    if (fb->dir == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }

    devfs_file_desc_t files[] = {
        {
            .name = "name",
            .vnodeOps = NULL,
            .fileOps = &nameOps,
            .data = fb,
        },
        {
            .name = "info",
            .vnodeOps = NULL,
            .fileOps = &infoOps,
            .data = fb,
        },
        {
            .name = "data",
            .vnodeOps = NULL,
            .fileOps = &dataOps,
            .data = fb,
        },
        {
            .name = NULL,
        },
    };

    if (!devfs_files_new(&fb->files, fb->dir, files))
    {
        UNREF(fb->dir);
        return ERR(DRIVER, NOMEM);
    }

    LOG_INFO("new framebuffer device `%s` with id '%s'\n", fb->name, id);
    return OK;
}

void fb_unregister(fb_t* fb)
{
    if (fb == NULL)
    {
        return;
    }

    UNREF(fb->dir);
    devfs_files_free(&fb->files);
}
