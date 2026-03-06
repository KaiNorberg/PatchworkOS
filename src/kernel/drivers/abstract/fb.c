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

static status_t fb_name_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    uint64_t length = strlen(fb->name);
    return irp_read_helper(irp, fb->name, length);
}

static vnode_class_t nameClass = {
    .name = "fb name",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = fb_name_read,
        },
};

static status_t fb_info_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    char string[256];
    int length = snprintf(string, sizeof(string), "%llu %llu %llu %s", fb->width, fb->height, fb->pitch, fb->format);

    if (length < 0 || (size_t)length >= sizeof(string))
    {
        return ERR(DRIVER, IMPL);
    }

    return irp_read_helper(irp, string, (size_t)length);
}

static vnode_class_t infoClass = {
    .name = "fb info",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = fb_info_read,
        },
};

static vnode_class_t dirClass = {
    .name = "fb dir",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

static vnode_class_t rootClass = {
    .name = "fb root",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

status_t fb_register(fb_t* fb)
{
    if (fb == NULL || fb->name == NULL || fb->format == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    if (dir == NULL)
    {
        dir = devfs_dentry_new(NULL, "fb", &dirClass, NULL);
        if (dir == NULL)
        {
            return ERR(DRIVER, NOMEM);
        }
    }

    char id[MAX_NAME];
    snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1));

    fb->internal.dir = devfs_dentry_new(dir, id, &dirClass, fb);
    if (fb->internal.dir == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }

    fb->internal.name = devfs_dentry_new(fb->internal.dir, "name", &nameClass, fb);
    if (fb->internal.name == NULL)
    {
        UNREF(fb->internal.dir);
        return ERR(DRIVER, NOMEM);
    }

    fb->internal.info = devfs_dentry_new(fb->internal.dir, "info", &infoClass, fb);
    if (fb->internal.info == NULL)
    {
        UNREF(fb->internal.dir);
        UNREF(fb->internal.name);
        return ERR(DRIVER, NOMEM);
    }

    fb->internal.data = devfs_dentry_new(fb->internal.dir, "data", fb->data, NULL);
    if (fb->internal.data == NULL)
    {
        UNREF(fb->internal.dir);
        UNREF(fb->internal.name);
        UNREF(fb->internal.info);
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

    UNREF(fb->internal.dir);
    UNREF(fb->internal.name);
    UNREF(fb->internal.info);
    UNREF(fb->internal.data);
}
