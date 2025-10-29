#include <kernel/drivers/abstractions/mouse.h>
#include <kernel/fs/file.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/math.h>

static dentry_t* mouseDir = NULL;

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static uint64_t mouse_events_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    mouse_t* mouse = file->inode->private;

    count = ROUND_DOWN(count, sizeof(mouse_event_t));
    for (uint64_t i = 0; i < count / sizeof(mouse_event_t); i++)
    {
        LOCK_SCOPE(&mouse->lock);

        if (WAIT_BLOCK_LOCK(&mouse->waitQueue, &mouse->lock, *offset != mouse->writeIndex) == ERR)
        {
            return i * sizeof(mouse_event_t);
        }

        ((mouse_event_t*)buffer)[i] = mouse->events[*offset];
        *offset = (*offset + 1) % MOUSE_MAX_EVENT;
    }

    return count;
}

static wait_queue_t* mouse_events_poll(file_t* file, poll_events_t* revents)
{
    mouse_t* mouse = file->inode->private;
    LOCK_SCOPE(&mouse->lock);
    if (mouse->writeIndex != file->pos)
    {
        *revents |= POLLIN;
    }
    return &mouse->waitQueue;
}

static file_ops_t eventsOps = {
    .read = mouse_events_read,
    .poll = mouse_events_poll,
};

static uint64_t mouse_name_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    mouse_t* mouse = file->inode->private;
    uint64_t nameLen = strnlen_s(mouse->name, MAX_NAME);
    if (*offset >= nameLen)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, mouse->name, nameLen);
}

static file_ops_t nameOps = {
    .read = mouse_name_read,
};

static void mouse_dir_cleanup(inode_t* inode)
{
    mouse_t* mouse = inode->private;
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

    if (mouseDir == NULL)
    {
        mouseDir = sysfs_dir_new(NULL, "mouse", NULL, NULL);
        if (mouseDir == NULL)
        {
            return NULL;
        }
    }

    mouse_t* mouse = calloc(1, sizeof(mouse_t));
    if (mouse == NULL)
    {
        return NULL;
    }
    strncpy(mouse->name, name, MAX_NAME - 1);
    mouse->name[MAX_NAME - 1] = '\0';
    mouse->writeIndex = 0;
    wait_queue_init(&mouse->waitQueue);
    lock_init(&mouse->lock);

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        wait_queue_deinit(&mouse->waitQueue);
        free(mouse);
        return NULL;
    }

    mouse->dir = sysfs_dir_new(mouseDir, id, &dirInodeOps, mouse);
    if (mouse->dir == NULL)
    {
        wait_queue_deinit(&mouse->waitQueue);
        free(mouse);
        return NULL;
    }
    mouse->eventsFile = sysfs_file_new(mouse->dir, "events", NULL, &eventsOps, mouse);
    if (mouse->eventsFile == NULL)
    {
        DEREF(mouse->dir); // mouse will be freed in mouse_dir_cleanup
        return NULL;
    }
    mouse->nameFile = sysfs_file_new(mouse->dir, "name", NULL, &nameOps, mouse);
    if (mouse->nameFile == NULL)
    {
        DEREF(mouse->dir);
        DEREF(mouse->eventsFile);
        return NULL;
    }

    return mouse;
}

void mouse_free(mouse_t* mouse)
{
    DEREF(mouse->dir);
    // mouse will be freed in mouse_dir_cleanup
}

void mouse_push(mouse_t* mouse, mouse_buttons_t buttons, int64_t deltaX, int64_t deltaY)
{
    LOCK_SCOPE(&mouse->lock);
    mouse->events[mouse->writeIndex] = (mouse_event_t){
        .time = timer_uptime(),
        .buttons = buttons,
        .deltaX = deltaX,
        .deltaY = deltaY,
    };
    mouse->writeIndex = (mouse->writeIndex + 1) % MOUSE_MAX_EVENT;
    wait_unblock(&mouse->waitQueue, WAIT_ALL, EOK);
}
