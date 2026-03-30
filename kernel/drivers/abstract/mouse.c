#include <kernel/drivers/abstract/mouse.h>

#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/vfs.h>
#include <kernel/io/irp.h>
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

static status_t mouse_name_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    mouse_t* mouse = frame->vnode->data;
    assert(mouse != NULL);

    size_t length = strlen(mouse->name);
    return irp_read_helper(irp, mouse->name, length);
}

static vnode_class_t nameClass = {
    .name = "mouse name",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = mouse_name_read,
        },
};

static vnode_class_t eventsClass = {
    .name = "mouse events",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            STRINGSTREAM_HANDLERS(),
        },
};

static status_t mouse_dir_reclaim(irp_t* irp)
{
    mouse_t* mouse = irp_current(irp)->vnode->data;
    if (mouse == NULL)
    {
        return OK;
    }

    stringstream_deinit(&mouse->internal.stream);

    return OK;
}

static vnode_class_t dirClass = {
    .name = "mouse dir",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
            [IRP_MJ_RECLAIM] = mouse_dir_reclaim,
        },
};

static vnode_class_t rootClass = {
    .name = "mouse root",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

void mouse_init(void)
{
    root = devfs_dentry_new(NULL, "mouse", &rootClass, NULL);
    if (root == NULL)
    {
        panic(NULL, "Failed to create mouse root dentry");
    }
}

status_t mouse_register(mouse_t* mouse)
{
    if (mouse == NULL || mouse->name == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    stringstream_init(&mouse->internal.stream);
    mouse->internal.dir = NULL;
    list_init(&mouse->internal.files);

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        return ERR(DRIVER, IMPL);
    }

    mouse->internal.dir = devfs_dentry_new(root, id, &dirClass, mouse);
    if (mouse->internal.dir == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }

    devfs_desc_t files[] = {
        {
            .name = "name",
            .cls = &nameClass,
            .data = mouse,
        },
        {
            .name = "events",
            .cls = &eventsClass,
            .data = &mouse->internal.stream,
        },
    };

    if (!devfs_dentrys_new(&mouse->internal.files, mouse->internal.dir, files, ARRAY_SIZE(files)))
    {
        UNREF(mouse->internal.dir);
        return ERR(DRIVER, NOMEM);
    }

    return OK;
}

void mouse_unregister(mouse_t* mouse)
{
    if (mouse == NULL)
    {
        return;
    }

    UNREF(mouse->internal.dir);
    devfs_dentrys_free(&mouse->internal.files);
}

static void mouse_broadcast(mouse_t* mouse, const char* string, size_t length)
{
    stringstream_broadcast(&mouse->internal.stream, string, length);
}

void mouse_press(mouse_t* mouse, uint8_t button)
{
    if (mouse == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%+03u_", button);
    if (length < 0)
    {
        LOG_ERR("failed to format mouse press event\n");
        return;
    }

    mouse_broadcast(mouse, event, (size_t)length);
}

void mouse_release(mouse_t* mouse, uint8_t button)
{
    if (mouse == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%+03u^", button);
    if (length < 0)
    {
        LOG_ERR("failed to format mouse release event\n");
        return;
    }

    mouse_broadcast(mouse, event, (size_t)length);
}

void mouse_move_x(mouse_t* mouse, int8_t delta)
{
    if (mouse == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%+03hhdx", delta);
    if (length < 0)
    {
        LOG_ERR("failed to format mouse move X event\n");
        return;
    }

    mouse_broadcast(mouse, event, (size_t)length);
}

void mouse_move_y(mouse_t* mouse, int8_t delta)
{
    if (mouse == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%+03hhdy", delta);
    if (length < 0)
    {
        LOG_ERR("failed to format mouse move Y event\n");
        return;
    }

    mouse_broadcast(mouse, event, (size_t)length);
}

void mouse_scroll(mouse_t* mouse, int8_t delta)
{
    if (mouse == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%+03hhdz", delta);
    if (length < 0)
    {
        LOG_ERR("failed to format mouse scroll event\n");
        return;
    }

    mouse_broadcast(mouse, event, (size_t)length);
}
