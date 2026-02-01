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

static void fb_name_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    uint64_t length = strlen(fb->name);
    status_t status =
        mdl_read(frame->read.buffer, frame->read.count, frame->read.offset, &irp->result, fb->name, length);
    irp_complete(irp, status);
}

static vnode_class_t nameClass = {
    .name = "fb name",
    .type = VNODE_REGULAR,
    .handlers =
        {
            [IRP_MJ_READ] = fb_name_read,
        },
};

static void fb_data_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    if (fb->read == NULL)
    {
        irp_complete(irp, ERR(DRIVER, INVAL));
        return;
    }

    status_t status = fb->read(fb, frame->read.buffer, frame->read.count, frame->read.offset, &irp->result);
    irp_complete(irp, status);
}

static void fb_data_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    if (fb->write == NULL)
    {
        irp_complete(irp, ERR(DRIVER, INVAL));
        return;
    }

    status_t status = fb->write(fb, frame->write.buffer, frame->write.count, frame->write.offset, &irp->result);
    irp_complete(irp, status);
}

static void fb_data_mmap(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    if (fb->write == NULL)
    {
        irp_complete(irp, ERR(DRIVER, INVAL));
        return;
    }

    void* addr = frame->mmap.address;
    status_t status = fb->mmap(fb, &addr, frame->mmap.length, frame->mmap.offset, frame->mmap.flags);
    irp->result = (uintptr_t)addr;
    irp_complete(irp, status);
}

static vnode_class_t dataClass = {
    .name = "fb data",
    .type = VNODE_REGULAR,
    .handlers =
        {
            [IRP_MJ_READ] = fb_data_read,
            [IRP_MJ_WRITE] = fb_data_write,
            [IRP_MJ_MMAP] = fb_data_mmap,
        },
};

static void fb_info_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    fb_t* fb = frame->vnode->data;
    assert(fb != NULL);

    if (fb->info == NULL)
    {
        irp_complete(irp, ERR(DRIVER, INVAL));
        return;
    }

    fb_info_t info = {0};
    status_t status = fb->info(fb, &info);
    if (IS_ERR(status))
    {
        irp_complete(irp, status);
        return;
    }

    char string[256];
    int length =
        snprintf(string, sizeof(string), "%llu %llu %llu %s", info.width, info.height, info.pitch, info.format);
    assert(length > 0);

    if ((size_t)length >= sizeof(string))
    {
        irp_complete(irp, ERR(DRIVER, IMPL));
        return;
    }

    status = mdl_read(frame->read.buffer, frame->read.count, frame->read.offset, &irp->result, string, length);
    irp_complete(irp, status);
}

static vnode_class_t infoClass = {
    .name = "fb info",
    .type = VNODE_REGULAR,
    .handlers =
        {
            [IRP_MJ_READ] = fb_info_read,
        },
};

static void fb_dir_cleanup(vnode_t* vnode)
{
    fb_t* fb = vnode->data;

    if (fb->cleanup != NULL)
    {
        fb->cleanup(fb);
    }
}

static vnode_class_t dirClass = {
    .name = "fb dir",
    .type = VNODE_DIR,
    .cleanup = fb_dir_cleanup,
};

static vnode_class_t rootClass = {
    .name = "fb root",
    .type = VNODE_DIR,
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
        }
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
