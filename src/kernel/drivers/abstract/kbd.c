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

#include <errno.h>
#include <kernel/utils/fifo.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/math.h>
#include <sys/proc.h>

static dentry_t* dir = NULL;

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static size_t kbd_name_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    kbd_t* kbd = file->vnode->data;
    assert(kbd != NULL);

    size_t length = strlen(kbd->name);
    return BUFFER_READ(buffer, count, offset, kbd->name, length);
}

static file_ops_t nameOps = {
    .read = kbd_name_read,
};

static uint64_t kbd_events_open(file_t* file)
{
    kbd_t* kbd = file->vnode->data;
    assert(kbd != NULL);

    kbd_client_t* client = calloc(1, sizeof(kbd_client_t));
    if (client == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }
    list_entry_init(&client->entry);
    fifo_init(&client->fifo, client->buffer, sizeof(client->buffer));

    lock_acquire(&kbd->lock);
    list_push_back(&kbd->clients, &client->entry);
    lock_release(&kbd->lock);

    file->data = client;
    return 0;
}

static void kbd_events_close(file_t* file)
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

static size_t kbd_events_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    UNUSED(offset);

    if (count == 0)
    {
        return 0;
    }

    kbd_t* kbd = file->vnode->data;
    assert(kbd != NULL);
    kbd_client_t* client = file->data;
    assert(client != NULL);

    LOCK_SCOPE(&kbd->lock);

    if (fifo_bytes_readable(&client->fifo) == 0)
    {
        if (file->mode & MODE_NONBLOCK)
        {
            errno = EAGAIN;
            return ERR;
        }

        if (WAIT_BLOCK_LOCK(&kbd->waitQueue, &kbd->lock, fifo_bytes_readable(&client->fifo) != 0) == ERR)
        {
            return ERR;
        }
    }

    return fifo_read(&client->fifo, buffer, count);
}

static wait_queue_t* kbd_events_poll(file_t* file, poll_events_t* revents)
{
    kbd_t* kbd = file->vnode->data;
    assert(kbd != NULL);
    kbd_client_t* client = file->data;
    assert(client != NULL);

    LOCK_SCOPE(&kbd->lock);

    if (fifo_bytes_readable(&client->fifo) != 0)
    {
        *revents |= POLLIN;
    }
    return &kbd->waitQueue;
}

static file_ops_t eventsOps = {
    .open = kbd_events_open,
    .close = kbd_events_close,
    .read = kbd_events_read,
    .poll = kbd_events_poll,
};

static void kbd_dir_cleanup(vnode_t* vnode)
{
    kbd_t* kbd = vnode->data;
    if (kbd == NULL)
    {
        return;
    }

    wait_queue_deinit(&kbd->waitQueue);
    free(kbd);
}

static vnode_ops_t dirVnodeOps = {
    .cleanup = kbd_dir_cleanup,
};

kbd_t* kbd_new(const char* name)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (dir == NULL)
    {
        dir = devfs_dir_new(NULL, "kbd", NULL, NULL);
        if (dir == NULL)
        {
            return NULL;
        }
    }

    kbd_t* kbd = malloc(sizeof(kbd_t));
    if (kbd == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    strncpy(kbd->name, name, sizeof(kbd->name));
    kbd->name[sizeof(kbd->name) - 1] = '\0';
    wait_queue_init(&kbd->waitQueue);
    list_init(&kbd->clients);
    lock_init(&kbd->lock);
    kbd->dir = NULL;
    list_init(&kbd->files);

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        wait_queue_deinit(&kbd->waitQueue);
        free(kbd);
        errno = EIO;
        return NULL;
    }

    kbd->dir = devfs_dir_new(dir, id, &dirVnodeOps, kbd);
    if (kbd->dir == NULL)
    {
        wait_queue_deinit(&kbd->waitQueue);
        free(kbd);
        return NULL;
    }

    devfs_file_desc_t files[] = {
        {
            .name = "name",
            .fileOps = &nameOps,
            .data = kbd,
        },
        {
            .name = "events",
            .fileOps = &eventsOps,
            .data = kbd,
        },
        {
            .name = NULL,
        },
    };

    if (devfs_files_new(&kbd->files, kbd->dir, files) == ERR)
    {
        wait_queue_deinit(&kbd->waitQueue);
        UNREF(kbd->dir);
        free(kbd);
        return NULL;
    }

    return kbd;
}

void kbd_free(kbd_t* kbd)
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
    LOCK_SCOPE(&kbd->lock);

    kbd_client_t* client;
    LIST_FOR_EACH(client, &kbd->clients, entry)
    {
        if (fifo_bytes_writeable(&client->fifo) >= length)
        {
            fifo_write(&client->fifo, string, length);
        }
    }

    wait_unblock(&kbd->waitQueue, WAIT_ALL, EOK);
}

void kbd_press(kbd_t* kbd, keycode_t code)
{
    if (kbd == NULL)
    {
        return;
    }

    char event[MAX_NAME];
    int length = snprintf(event, sizeof(event), "%u_", code);
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
    int length = snprintf(event, sizeof(event), "%u^", code);
    if (length < 0)
    {
        LOG_ERR("failed to format keyboard release event\n");
        return;
    }

    kbd_broadcast(kbd, event, (size_t)length);
}