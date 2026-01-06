#include <kernel/drivers/abstract/kbd.h>

#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/ring.h>
#include <kernel/log/log.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

static dentry_t* dir = NULL;

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static uint64_t kbd_name_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    kbd_t* kbd = file->inode->private;
    assert(kbd != NULL);

    uint64_t length = strlen(kbd->name);
    return BUFFER_READ(buffer, count, offset, kbd->name, length);
}

static file_ops_t nameOps = {
    .read = kbd_name_read,
};

static uint64_t kbd_events_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    kbd_t* kbd = file->inode->private;
    assert(kbd != NULL);

    if (count >= sizeof(kbd->buffer))
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&kbd->lock);

    if (ring_bytes_used(&kbd->events, offset) == 0)
    {
        if (file->mode & MODE_NONBLOCK)
        {
            errno = EAGAIN;
            return ERR;
        }

        if (WAIT_BLOCK_LOCK(&kbd->waitQueue, &kbd->lock, ring_bytes_used(&kbd->events, offset) > 0))
        {
            return ERR;
        }
    }

    uint64_t result = ring_read(&kbd->events, buffer, count, offset);
    return result;
}

static wait_queue_t* kbd_events_poll(file_t* file, poll_events_t* revents)
{
    kbd_t* kbd = file->inode->private;
    assert(kbd != NULL);

    LOCK_SCOPE(&kbd->lock);

    if (ring_bytes_used(&kbd->events, &file->pos) > 0)
    {
        *revents |= POLLIN;
    }
    return &kbd->waitQueue;
}

static file_ops_t eventsOps = {
    .read = kbd_events_read,
    .poll = kbd_events_poll,
};

static void kbd_dir_cleanup(inode_t* inode)
{
    kbd_t* kbd = inode->private;
    if (kbd == NULL)
    {
        return;
    }

    free(kbd);
}

static inode_ops_t dirInodeOps = {
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
    ring_init(&kbd->events, kbd->buffer, sizeof(kbd->buffer));
    wait_queue_init(&kbd->waitQueue);
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

    kbd->dir = devfs_dir_new(dir, id, &dirInodeOps, kbd);
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
            .private = kbd,
        },
        {
            .name = "events",
            .fileOps = &eventsOps,
            .private = kbd,
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

void kbd_press(kbd_t* kbd, keycode_t code)
{
    if (kbd == NULL)
    {
        return;
    }

    LOCK_SCOPE(&kbd->lock);

    char string[MAX_NAME];
    int length = snprintf(string, MAX_NAME, "_%u\n", code);
    if (length < 0)
    {
        return;
    }
    ring_write(&kbd->events, string, (uint64_t)length, NULL);
    wait_unblock(&kbd->waitQueue, WAIT_ALL, EOK);
}

void kbd_release(kbd_t* kbd, keycode_t code)
{
    if (kbd == NULL)
    {
        return;
    }

    LOCK_SCOPE(&kbd->lock);

    char string[MAX_NAME];
    int length = snprintf(string, MAX_NAME, "^%u\n", code);
    if (length < 0)
    {
        return;
    }
    ring_write(&kbd->events, string, (uint64_t)length, NULL);
    wait_unblock(&kbd->waitQueue, WAIT_ALL, EOK);
}