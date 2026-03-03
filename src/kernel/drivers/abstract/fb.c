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
    .type = VTYPE_DEVICE,
    VNODE_HANDLERS([IRP_MJ_READ] = fb_name_read),
};

static status_t fb_data_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    if (fb->read == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    return fb->read(irp);
}

static status_t fb_data_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    if (fb->write == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    return fb->write(irp);
}

static status_t fb_data_mmap(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    if (fb->mmap == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    return fb->mmap(irp);
}

static vnode_class_t dataClass = {
    .name = "fb data",
    .type = VTYPE_DEVICE,
    VNODE_HANDLERS([IRP_MJ_READ] = fb_data_read, [IRP_MJ_WRITE] = fb_data_write, [IRP_MJ_MMAP] = fb_data_mmap),
};

static status_t fb_info_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    if (fb->info == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    fb_info_t info = {0};
    status_t status = fb->info(fb, &info);
    if (IS_ERR(status))
    {
        return status;
    }

    char string[256];
    int length =
        snprintf(string, sizeof(string), "%llu %llu %llu %s", info.width, info.height, info.pitch, info.format);

    if (length < 0 || (size_t)length >= sizeof(string))
    {
        return ERR(DRIVER, IMPL);
    }

    return irp_read_helper(irp, string, (size_t)length);
}

static vnode_class_t infoClass = {
    .name = "fb info",
    .type = VTYPE_DEVICE,
    VNODE_HANDLERS([IRP_MJ_READ] = fb_info_read),
};

static status_t fb_dir_reclaim(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    if (fb->reclaim == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    return fb->reclaim(irp);
}

static vnode_class_t dirClass = {
    .name = "fb dir",
    .type = VTYPE_DIRECTORY,
    VNODE_DIR_HANDLERS([IRP_MJ_RECLAIM] = fb_dir_reclaim),
};

static vnode_class_t rootClass = {
    .name = "fb root",
    .type = VTYPE_DIRECTORY,
    VNODE_DIR_HANDLERS(),
};

status_t fb_register(fb_t* fb)
{
    if (fb == NULL || fb->name == NULL || fb->info == NULL)
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

    list_init(&fb->files);

    char id[MAX_NAME];
    snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1));

    fb->dir = devfs_dentry_new(dir, id, &dirClass, fb);
    if (fb->dir == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }

    devfs_desc_t files[] = {
        {
            .name = "name",
            .cls = &nameClass,
            .data = fb,
        },
        {
            .name = "info",
            .cls = &infoClass,
            .data = fb,
        },
        {
            .name = "data",
            .cls = &dataClass,
            .data = fb,
        },
    };

    if (!devfs_dentrys_new(&fb->files, fb->dir, files, ARRAY_SIZE(files)))
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
    devfs_dentrys_free(&fb->files);
}
