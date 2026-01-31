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

static void kbd_name_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    kbd_t* kbd = frame->vnode->data;
    assert(kbd != NULL);

    size_t length = strlen(kbd->name);
    status_t status =
        mdl_read(frame->read.buffer, frame->read.count, frame->read.offset, &irp->res.read, kbd->name, length);
    irp_complete(irp, status);
}

static vnode_class_t nameClass = {
    .name = "kbd name",
    .handlers =
        {
            [IRP_MJ_READ] = kbd_name_read,
        },
};

static status_t kbd_events_file_ctor(file_t* file)
{
    kbd_t* kbd = file->vnode->data;
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

static void kbd_events_file_dtor(file_t* file)
{
    kbd_t* kbd = file->vnode->data;
    assert(kbd != NULL);

    kbd_client_t* client = file->data;
    if (client == NULL)
    {
        return;
    }

    lock_acquire(&kbd->lock);
    list_remove(&client->entry);
    lock_release(&kbd->lock);

    free(client);
}

static void kbd_events_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    kbd_t* kbd = frame->vnode->data;
    assert(kbd != NULL);

    kbd_client_t* client = frame->read.file->data;
    assert(client != NULL);

    lock_acquire(&kbd->lock);

    if (fifo_bytes_readable(&client->fifo) == 0)
    {
        status_t status = irp_delay(irp, &kbd->pending, kbd_cancel);
        lock_release(&kbd->lock);
        if (IS_ERR(status))
        {
            irp_complete(irp, status);
        }
        return;
    }

    status_t status = fifo_read(&client->fifo, frame->read.buffer, frame->read.count, &irp->res.read);
    lock_release(&kbd->lock);
    irp_complete(irp, status);
}

static void kbd_events_poll(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    kbd_t* kbd = frame->vnode->data;
    assert(kbd != NULL);
    kbd_client_t* client = frame->poll.file->data;
    assert(client != NULL);

    lock_acquire(&kbd->lock);

    if (fifo_bytes_readable(&client->fifo) > 0)
    {
        irp->res.events = IOPOLL_READ;
        lock_release(&kbd->lock);
        irp_complete(irp, OK);
        return;
    }

    status_t status = irp_delay(irp, &kbd->pending, kbd_cancel);
    lock_release(&kbd->lock);

    if (IS_ERR(status))
    {
        irp->res.events = 0;
        irp_complete(irp, status);
    }
}

static vnode_class_t eventsClass = {
    .name = "kbd events",
    .file_ctor = kbd_events_file_ctor,
    .file_dtor = kbd_events_file_dtor,
    .handlers =
        {
            [IRP_MJ_READ] = kbd_events_read,
            [IRP_MJ_POLL] = kbd_events_poll,
        },
};

static void kbd_dir_cleanup(vnode_t* vnode)
{
    kbd_t* kbd = vnode->data;
    if (kbd == NULL)
    {
        return;
    }

    if (!list_is_empty(&kbd->pending))
    {
        panic(NULL, "Attempted to free keyboard with pending IRPs");
    }
    if (!list_is_empty(&kbd->clients))
    {
        panic(NULL, "Attempted to free keyboard with clients");
    }
}

static vnode_class_t dirClass = {
    .name = "kbd dir",
    .cleanup = kbd_dir_cleanup,
};

status_t kbd_register(kbd_t* kbd)
{
    if (kbd == NULL || kbd->name == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    if (dir == NULL)
    {
        dir = devfs_dir_new(NULL, "kbd", NULL, NULL);
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

    kbd->dir = devfs_dir_new(dir, id, &dirClass, kbd);
    if (kbd->dir == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }

    devfs_file_desc_t files[] = {
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
    if (!devfs_files_new(&kbd->files, kbd->dir, files, ARRAY_SIZE(files)))
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
    devfs_files_free(&kbd->files);
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
            kbd_events_read(irp);
        }
        else if (frame->major == IRP_MJ_POLL)
        {
            kbd_events_poll(irp);
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