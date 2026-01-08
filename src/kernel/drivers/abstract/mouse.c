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

static size_t mouse_name_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    mouse_t* mouse = file->inode->private;
    assert(mouse != NULL);

    size_t length = strlen(mouse->name);
    return BUFFER_READ(buffer, count, offset, mouse->name, length);
}

static file_ops_t nameOps = {
    .read = mouse_name_read,
};

static uint64_t mouse_events_open(file_t* file)
{
    mouse_t* mouse = file->inode->private;
    assert(mouse != NULL);

    mouse_client_t* client = calloc(1, sizeof(mouse_client_t));
    if (client == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }
    list_entry_init(&client->entry);
    fifo_init(&client->fifo, client->buffer, sizeof(client->buffer));

    lock_acquire(&mouse->lock);
    list_push_back(&mouse->clients, &client->entry);
    lock_release(&mouse->lock);

    file->private = client;
    return 0;
}

static void mouse_events_close(file_t* file)
{
    mouse_t* mouse = file->inode->private;
    assert(mouse != NULL);

    mouse_client_t* client = file->private;
    if (client == NULL)
    {
        return;
    }

    lock_acquire(&mouse->lock);
    list_remove(&mouse->clients, &client->entry);
    lock_release(&mouse->lock);

    free(client);
}

static size_t mouse_events_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    UNUSED(offset);

    if (count == 0)
    {
        return 0;
    }

    mouse_t* mouse = file->inode->private;
    assert(mouse != NULL);
    mouse_client_t* client = file->private;
    assert(client != NULL);

    LOCK_SCOPE(&mouse->lock);

    if (fifo_bytes_readable(&client->fifo) == 0)
    {
        if (file->mode & MODE_NONBLOCK)
        {
            errno = EAGAIN;
            return ERR;
        }

        if (WAIT_BLOCK_LOCK(&mouse->waitQueue, &mouse->lock, fifo_bytes_readable(&client->fifo) != 0) == ERR)
        {
            return ERR;
        }
    }

    return fifo_read(&client->fifo, buffer, count);
}

static wait_queue_t* mouse_events_poll(file_t* file, poll_events_t* revents)
{
    mouse_t* mouse = file->inode->private;
    assert(mouse != NULL);
    mouse_client_t* client = file->private;
    assert(client != NULL);

    LOCK_SCOPE(&mouse->lock);

    if (fifo_bytes_readable(&client->fifo) != 0)
    {
        *revents |= POLLIN;
    }
    return &mouse->waitQueue;
}

static file_ops_t eventsOps = {
    .open = mouse_events_open,
    .close = mouse_events_close,
    .read = mouse_events_read,
    .poll = mouse_events_poll,
};

static void mouse_dir_cleanup(inode_t* inode)
{
    mouse_t* mouse = inode->private;
    if (mouse == NULL)
    {
        return;
    }

    wait_queue_deinit(&mouse->waitQueue);
    free(mouse);
}

static inode_ops_t dirInodeOps = {
    .cleanup = mouse_dir_cleanup,
};

mouse_t* mouse_new(const char* name)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (dir == NULL)
    {
        dir = devfs_dir_new(NULL, "mouse", NULL, NULL);
        if (dir == NULL)
        {
            return NULL;
        }
    }

    mouse_t* mouse = malloc(sizeof(mouse_t));
    if (mouse == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    strncpy(mouse->name, name, sizeof(mouse->name));
    mouse->name[sizeof(mouse->name) - 1] = '\0';
    wait_queue_init(&mouse->waitQueue);
    list_init(&mouse->clients);
    lock_init(&mouse->lock);
    mouse->dir = NULL;
    list_init(&mouse->files);

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        wait_queue_deinit(&mouse->waitQueue);
        free(mouse->name);
        free(mouse);
        return NULL;
    }

    mouse->dir = devfs_dir_new(dir, id, &dirInodeOps, mouse);
    if (mouse->dir == NULL)
    {
        wait_queue_deinit(&mouse->waitQueue);
        free(mouse);
        return NULL;
    }

    devfs_file_desc_t files[] = {
        {
            .name = "name",
            .fileOps = &nameOps,
            .private = mouse,
        },
        {
            .name = "events",
            .fileOps = &eventsOps,
            .private = mouse,
        },
        {
            .name = NULL,
        },
    };

    if (devfs_files_new(&mouse->files, mouse->dir, files) == ERR)
    {
        wait_queue_deinit(&mouse->waitQueue);
        UNREF(mouse->dir);
        free(mouse);
        return NULL;
    }

    return mouse;
}

void mouse_free(mouse_t* mouse)
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