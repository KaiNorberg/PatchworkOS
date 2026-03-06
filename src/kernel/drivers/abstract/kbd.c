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
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/math.h>
#include <sys/proc.h>

static dentry_t* dir = NULL;

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static status_t kbd_cancel(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    kbd_t* kbd = frame->vnode->data;

    lock_acquire(&kbd->lock);

    if (list_contains(&irp->entry))
    {
        list_remove(&irp->entry);
    }

    lock_release(&kbd->lock);

    return OK;
}

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

static status_t kbd_events_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (frame->file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }

    kbd_t* kbd = frame->vnode->data;
    assert(kbd != NULL);

    kbd_client_t* client = calloc(1, sizeof(kbd_client_t));
    if (client == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }
    list_entry_init(&client->entry);
    fifo_init(&client->fifo, client->buffer, sizeof(client->buffer));

    lock_acquire(&kbd->lock);
    list_push_back(&kbd->clients, &client->entry);
    lock_release(&kbd->lock);

    file->data = client;
    return OK;
}

static status_t kbd_events_close(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (frame->file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }
    kbd_t* kbd = frame->vnode->data;
    assert(kbd != NULL);

    kbd_client_t* client = file->data;
    lock_acquire(&kbd->lock);
    list_remove(&client->entry);
    lock_release(&kbd->lock);

    free(client);
    return OK;
}

static status_t kbd_events_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    kbd_t* kbd = frame->vnode->data;
    assert(kbd != NULL);

    if (frame->file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }

    kbd_client_t* client = frame->file->data;
    assert(client != NULL);

    LOCK_SCOPE(&kbd->lock);

    if (fifo_bytes_readable(&client->fifo) == 0)
    {
        return irp_delay(irp, &kbd->pending, kbd_cancel);
    }

    return fifo_read_mdl(&client->fifo, frame->read.buffer, 0, &irp->result);
}

static status_t kbd_events_poll(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    kbd_t* kbd = frame->vnode->data;
    assert(kbd != NULL);

    if (frame->file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }

    kbd_client_t* client = frame->file->data;
    assert(client != NULL);

    LOCK_SCOPE(&kbd->lock);

    if (fifo_bytes_readable(&client->fifo) > 0)
    {
        irp->result = IOEVENT_READ;
        return OK;
    }

    return irp_delay(irp, &kbd->pending, kbd_cancel);
}

static vnode_class_t eventsClass = {
    .name = "kbd events",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = kbd_events_open,
            [IRP_MJ_READ] = kbd_events_read,
            [IRP_MJ_POLL] = kbd_events_poll,
            [IRP_MJ_CLOSE] = kbd_events_close,
        },
};

static status_t kbd_dir_reclaim(irp_t* irp)
{
    kbd_t* kbd = irp_current(irp)->vnode->data;
    if (kbd == NULL)
    {
        return OK;
    }

    if (!list_is_empty(&kbd->pending))
    {
        panic(NULL, "Attempted to free keyboard with pending IRPs");
    }
    if (!list_is_empty(&kbd->clients))
    {
        panic(NULL, "Attempted to free keyboard with clients");
    }

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

static vnode_class_t rootClass = {.name = "kbd root",
    .type = FILE_TYPE_DIRECTORY,
    .handlers = {
        VNODE_DIR_HANDLERS(),
    },};

status_t kbd_register(kbd_t* kbd)
{
    if (kbd == NULL || kbd->name == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    if (dir == NULL)
    {
        dir = devfs_dentry_new(NULL, "kbd", &rootClass, NULL);
        if (dir == NULL)
        {
            return ERR(DRIVER, NOMEM);
        }
    }

    list_init(&kbd->pending);
    list_init(&kbd->clients);
    lock_init(&kbd->lock);
    kbd->dir = NULL;
    list_init(&kbd->files);

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        return ERR(DRIVER, IMPL);
    }

    kbd->dir = devfs_dentry_new(dir, id, &dirClass, kbd);
    if (kbd->dir == NULL)
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
            .data = kbd,
        },
    };
    if (!devfs_dentrys_new(&kbd->files, kbd->dir, files, ARRAY_SIZE(files)))
    {
        UNREF(kbd->dir);
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

    UNREF(kbd->dir);
    devfs_dentrys_free(&kbd->files);
}

static void kbd_broadcast(kbd_t* kbd, const char* string, size_t length)
{
    list_t pending = LIST_CREATE(pending);

    {
        LOCK_SCOPE(&kbd->lock);

        kbd_client_t* client;
        LIST_FOR_EACH(client, &kbd->clients, entry)
        {
            if (fifo_bytes_writeable(&client->fifo) >= length)
            {
                fifo_write(&client->fifo, string, length, NULL);
            }
        }
        irp_claim_list(&pending, &kbd->pending);
    }

    while (!list_is_empty(&pending))
    {
        irp_t* irp = CONTAINER_OF(list_pop_front(&pending), irp_t, entry);
        irp_frame_t* frame = irp_current(irp);
        if (frame->major == IRP_MJ_READ)
        {
            irp_complete(irp, kbd_events_read(irp));
        }
        else if (frame->major == IRP_MJ_POLL)
        {
            irp_complete(irp, kbd_events_poll(irp));
        }
        else
        {
            irp_complete(irp, ERR(DRIVER, INVAL));
        }
    }
}

void kbd_press(kbd_t* kbd, keycode_t code)
{
    if (kbd == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%03u_", code);
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
    int length = snprintf(event, sizeof(event), "%03u^", code);
    if (length < 0)
    {
        LOG_ERR("failed to format keyboard release event\n");
        return;
    }

    kbd_broadcast(kbd, event, (size_t)length);
}
