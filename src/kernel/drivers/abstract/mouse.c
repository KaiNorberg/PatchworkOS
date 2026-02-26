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
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/math.h>
#include <sys/proc.h>

static dentry_t* dir = NULL;

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static status_t mouse_cancel(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    mouse_t* mouse = frame->vnode->data;

    lock_acquire(&mouse->lock);

    if (list_contains(&irp->entry))
    {
        list_remove(&irp->entry);
    }

    lock_release(&mouse->lock);

    return OK;
}

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
            [IRP_MJ_READ] = mouse_name_read,
        },
};

static status_t mouse_events_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (frame->file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }

    mouse_t* mouse = file->vnode->data;
    assert(mouse != NULL);

    mouse_client_t* client = calloc(1, sizeof(mouse_client_t));
    if (client == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }
    list_entry_init(&client->entry);
    fifo_init(&client->fifo, client->buffer, sizeof(client->buffer));

    lock_acquire(&mouse->lock);
    list_push_back(&mouse->clients, &client->entry);
    lock_release(&mouse->lock);

    file->data = client;
    return OK;
}

static void mouse_events_close(file_t* file)
{
    mouse_t* mouse = file->vnode->data;
    assert(mouse != NULL);

    mouse_client_t* client = file->data;
    if (client == NULL)
    {
        return;
    }

    lock_acquire(&mouse->lock);
    list_remove(&client->entry);
    lock_release(&mouse->lock);

    free(client);
}

static status_t mouse_events_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    mouse_t* mouse = frame->vnode->data;
    assert(mouse != NULL);

    if (frame->file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }

    mouse_client_t* client = frame->file->data;
    assert(client != NULL);

    LOCK_SCOPE(&mouse->lock);

    if (fifo_bytes_readable(&client->fifo) == 0)
    {
        return irp_delay(irp, &mouse->pending, mouse_cancel);
    }

    return fifo_read_mdl(&client->fifo, frame->read.buffer, 0, &irp->result);
}

static status_t mouse_events_poll(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    mouse_t* mouse = frame->vnode->data;
    assert(mouse != NULL);

    if (frame->file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }

    mouse_client_t* client = frame->file->data;
    assert(client != NULL);

    LOCK_SCOPE(&mouse->lock);

    if (fifo_bytes_readable(&client->fifo) > 0)
    {
        irp->result = EVENTS_READ;
        return OK;
    }

    return irp_delay(irp, &mouse->pending, mouse_cancel);
}

static vnode_class_t eventsClass = {
    .name = "mouse events",
    .type = FILE_TYPE_DEVICE,
    .close = mouse_events_close,
    .handlers =
        {
            [IRP_MJ_OPEN] = mouse_events_open,
            [IRP_MJ_READ] = mouse_events_read,
            [IRP_MJ_POLL] = mouse_events_poll,
        },
};

static void mouse_dir_cleanup(vnode_t* vnode)
{
    mouse_t* mouse = vnode->data;
    if (mouse == NULL)
    {
        return;
    }

    if (!list_is_empty(&mouse->pending))
    {
        panic(NULL, "Attempted to free mouse with pending IRPs");
    }
    if (!list_is_empty(&mouse->clients))
    {
        panic(NULL, "Attempted to free mouse with clients");
    }
}

static vnode_class_t dirClass = {
    .name = "mouse dir",
    .type = FILE_TYPE_DIRECTORY,
    .cleanup = mouse_dir_cleanup,
    .handlers =
        {
            [IRP_MJ_READ] = vnode_generic_dir_read,
        },
};

static vnode_class_t rootClass = {
    .name = "mouse root",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            [IRP_MJ_READ] = vnode_generic_dir_read,
        },
};

status_t mouse_register(mouse_t* mouse)
{
    if (mouse == NULL || mouse->name == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    if (dir == NULL)
    {
        dir = devfs_dentry_new(NULL, "mouse", &rootClass, NULL);
        if (dir == NULL)
        {
            return ERR(DRIVER, NOMEM);
        }
    }

    list_init(&mouse->pending);
    list_init(&mouse->clients);
    lock_init(&mouse->lock);
    mouse->dir = NULL;
    list_init(&mouse->files);

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        return ERR(DRIVER, IMPL);
    }

    mouse->dir = devfs_dentry_new(dir, id, &dirClass, mouse);
    if (mouse->dir == NULL)
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
            .data = mouse,
        },
    };

    if (!devfs_dentrys_new(&mouse->files, mouse->dir, files, ARRAY_SIZE(files)))
    {
        UNREF(mouse->dir);
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

    UNREF(mouse->dir);
    devfs_dentrys_free(&mouse->files);
}

static void mouse_broadcast(mouse_t* mouse, const char* string, size_t length)
{
    list_t pending = LIST_CREATE(pending);

    {
        LOCK_SCOPE(&mouse->lock);

        mouse_client_t* client;
        LIST_FOR_EACH(client, &mouse->clients, entry)
        {
            if (fifo_bytes_writeable(&client->fifo) >= length)
            {
                fifo_write(&client->fifo, string, length, NULL);
            }
        }
        irp_claim_list(&pending, &mouse->pending);
    }

    while (!list_is_empty(&pending))
    {
        irp_t* irp = CONTAINER_OF(list_pop_front(&pending), irp_t, entry);
        irp_frame_t* frame = irp_current(irp);
        if (frame->major == IRP_MJ_READ)
        {
            status_t status = mouse_events_read(irp);
            if (IS_INFO(status) && IS_CODE(status, PENDING))
            {
                continue;
            }
            irp_complete(irp, status);
        }
        else if (frame->major == IRP_MJ_POLL)
        {
            status_t status = mouse_events_poll(irp);
            if (IS_INFO(status) && IS_CODE(status, PENDING))
            {
                continue;
            }
            irp_complete(irp, status);
        }
        else
        {
            irp_complete(irp, ERR(DRIVER, INVAL));
        }
    }
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