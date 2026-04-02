#include <kernel/drivers/abstract/kbd.h>

#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

#include <kernel/utils/fifo.h>
#include <libc/fs.h>
#include <libc/math.h>
#include <libc/proc.h>
#include <stdio.h>
#include <stdlib.h>

static dentry_t* root = NULL;

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static status_t kbd_name_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    kbd_t* kbd = frame->vnode->data;
    assert(kbd != NULL);

    size_t length = strlen(kbd->name);
    return irp_read_helper(irp, kbd->name, length);
}

static vnode_class_t nameClass = {
    .name = "kbd name",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = kbd_name_read,
        },
};

static vnode_class_t eventsClass = {
    .name = "kbd events",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            STRINGSTREAM_HANDLERS(),
        },
};

static status_t kbd_dir_reclaim(irp_t* irp)
{
    kbd_t* kbd = irp_current(irp)->vnode->data;
    if (kbd == NULL)
    {
        return OK;
    }

    stringstream_deinit(&kbd->internal.stream);

    return OK;
}

static vnode_class_t dirClass = {
    .name = "kbd dir",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
            [IRP_MJ_RECLAIM] = kbd_dir_reclaim,
        },
};

static vnode_class_t rootClass = {
    .name = "kbd root",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

void kbd_init(void)
{
    root = devfs_dentry_new(NULL, "kbd", &rootClass, NULL);
    if (root == NULL)
    {
        panic(NULL, "Failed to create kbd root dentry");
    }
}

status_t kbd_register(kbd_t* kbd)
{
    if (kbd == NULL || kbd->name == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    stringstream_init(&kbd->internal.stream);
    kbd->internal.dir = NULL;
    list_init(&kbd->internal.files);

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        return ERR(DRIVER, IMPL);
    }

    kbd->internal.dir = devfs_dentry_new(root, id, &dirClass, kbd);
    if (kbd->internal.dir == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }

    devfs_desc_t files[] = {
        {
            .name = "name",
            .cls = &nameClass,
            .data = kbd,
        },
        {
            .name = "events",
            .cls = &eventsClass,
            .data = &kbd->internal.stream,
        },
    };
    if (!devfs_dentrys_new(&kbd->internal.files, kbd->internal.dir, files, ARRAY_SIZE(files)))
    {
        UNREF(kbd->internal.dir);
        return ERR(DRIVER, NOMEM);
    }

    return OK;
}

void kbd_unregister(kbd_t* kbd)
{
    if (kbd == NULL)
    {
        return;
    }

    UNREF(kbd->internal.dir);
    devfs_dentrys_free(&kbd->internal.files);
}

static void kbd_broadcast(kbd_t* kbd, const char* string, size_t length)
{
    stringstream_broadcast(&kbd->internal.stream, string, length);
}

void kbd_press(kbd_t* kbd, keycode_t code)
{
    if (kbd == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%03u_", code % 1000);
    if (length < 0)
    {
        LOG_ERR("failed to format keyboard press event\n");
        return;
    }

    kbd_broadcast(kbd, event, (size_t)length);
}

void kbd_release(kbd_t* kbd, keycode_t code)
{
    if (kbd == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%03u^", code % 1000);
    if (length < 0)
    {
        LOG_ERR("failed to format keyboard release event\n");
        return;
    }

    kbd_broadcast(kbd, event, (size_t)length);
}
