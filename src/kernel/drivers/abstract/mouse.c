#include <kernel/drivers/abstract/mouse.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/math.h>

static dentry_t* dir = NULL;

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static status_t mouse_name_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    mouse_t* mouse = file->vnode->data;
    assert(mouse != NULL);

    size_t length = strlen(mouse->name);
    *bytesRead = BUFFER_READ(buffer, count, offset, mouse->name, length);
    return OK;
}

static file_ops_t nameOps = {
    .read = mouse_name_read,
};

static status_t mouse_events_open(file_t* file)
{
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

static status_t mouse_events_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    UNUSED(offset);

    if (count == 0)
    {
        *bytesRead = 0;
        return OK;
    }

    mouse_t* mouse = file->vnode->data;
    assert(mouse != NULL);
    mouse_client_t* client = file->data;
    assert(client != NULL);

    LOCK_SCOPE(&mouse->lock);

    if (fifo_bytes_readable(&client->fifo) == 0)
    {
        if (file->mode & MODE_NONBLOCK)
        {
            return INFO(DRIVER, AGAIN);
        }

        status_t status = WAIT_BLOCK_LOCK(&mouse->waitQueue, &mouse->lock, fifo_bytes_readable(&client->fifo) != 0);
        if (!IS_ERR(status))
        {
            return status;
        }
    }

    *bytesRead = fifo_read(&client->fifo, buffer, count);
    return OK;
}

static status_t mouse_events_poll(file_t* file, poll_events_t* revents, wait_queue_t** queue)
{
    mouse_t* mouse = file->vnode->data;
    assert(mouse != NULL);
    mouse_client_t* client = file->data;
    assert(client != NULL);

    LOCK_SCOPE(&mouse->lock);

    if (fifo_bytes_readable(&client->fifo) != 0)
    {
        *revents |= POLLIN;
    }
    *queue = &mouse->waitQueue;
    return OK;
}

static file_ops_t eventsOps = {
    .open = mouse_events_open,
    .close = mouse_events_close,
    .read = mouse_events_read,
    .poll = mouse_events_poll,
};

static void mouse_dir_cleanup(vnode_t* vnode)
{
    mouse_t* mouse = vnode->data;
    if (mouse == NULL)
    {
        return;
    }

    wait_queue_deinit(&mouse->waitQueue);
}

static vnode_ops_t dirVnodeOps = {
    .cleanup = mouse_dir_cleanup,
};

status_t mouse_register(mouse_t* mouse)
{
    if (mouse == NULL || mouse->name == NULL)
    {
        return ERR(DRIVER, INVAL);
    }

    if (dir == NULL)
    {
        dir = devfs_dir_new(NULL, "mouse", NULL, NULL);
        if (dir == NULL)
        {
            return ERR(DRIVER, NOMEM);
        }
    }

    wait_queue_init(&mouse->waitQueue);
    list_init(&mouse->clients);
    lock_init(&mouse->lock);
    mouse->dir = NULL;
    list_init(&mouse->files);

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        wait_queue_deinit(&mouse->waitQueue);
        return ERR(DRIVER, IMPL);
    }

    mouse->dir = devfs_dir_new(dir, id, &dirVnodeOps, mouse);
    if (mouse->dir == NULL)
    {
        wait_queue_deinit(&mouse->waitQueue);
        return ERR(DRIVER, NOMEM);
    }

    devfs_file_desc_t files[] = {
        {
            .name = "name",
            .fileOps = &nameOps,
            .data = mouse,
        },
        {
            .name = "events",
            .fileOps = &eventsOps,
            .data = mouse,
        },
        {
            .name = NULL,
        },
    };

    if (!devfs_files_new(&mouse->files, mouse->dir, files))
    {
        wait_queue_deinit(&mouse->waitQueue);
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
    devfs_files_free(&mouse->files);
}

static void mouse_broadcast(mouse_t* mouse, const char* string, size_t length)
{
    LOCK_SCOPE(&mouse->lock);

    mouse_client_t* client;
    LIST_FOR_EACH(client, &mouse->clients, entry)
    {
        if (fifo_bytes_writeable(&client->fifo) >= length)
        {
            fifo_write(&client->fifo, string, length);
        }
    }

    wait_unblock(&mouse->waitQueue, WAIT_ALL, EOK);
}

void mouse_press(mouse_t* mouse, uint32_t button)
{
    if (mouse == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%u_", button);
    if (length < 0)
    {
        LOG_ERR("failed to format mouse press event\n");
        return;
    }

    mouse_broadcast(mouse, event, (size_t)length);
}

void mouse_release(mouse_t* mouse, uint32_t button)
{
    if (mouse == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%u^", button);
    if (length < 0)
    {
        LOG_ERR("failed to format mouse release event\n");
        return;
    }

    mouse_broadcast(mouse, event, (size_t)length);
}

void mouse_move_x(mouse_t* mouse, int64_t delta)
{
    if (mouse == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%lldx", delta);
    if (length < 0)
    {
        LOG_ERR("failed to format mouse move X event\n");
        return;
    }

    mouse_broadcast(mouse, event, (size_t)length);
}

void mouse_move_y(mouse_t* mouse, int64_t delta)
{
    if (mouse == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%lldy", delta);
    if (length < 0)
    {
        LOG_ERR("failed to format mouse move Y event\n");
        return;
    }

    mouse_broadcast(mouse, event, (size_t)length);
}

void mouse_scroll(mouse_t* mouse, int64_t delta)
{
    if (mouse == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%lldz", delta);
    if (length < 0)
    {
        LOG_ERR("failed to format mouse scroll event\n");
        return;
    }

    mouse_broadcast(mouse, event, (size_t)length);
}