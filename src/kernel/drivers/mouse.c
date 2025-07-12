#include "mouse.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "mem/heap.h"
#include "ps2/mouse.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "systime/systime.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/math.h>

static sysfs_dir_t mouseDir = {0};

static uint64_t mouse_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    mouse_t* mouse = file->inode->private;

    count = ROUND_DOWN(count, sizeof(mouse_event_t));
    for (uint64_t i = 0; i < count / sizeof(mouse_event_t); i++)
    {
        LOCK_SCOPE(&mouse->lock);

        if (WAIT_BLOCK_LOCK(&mouse->waitQueue, &mouse->lock, *offset != mouse->writeIndex) != WAIT_NORM)
        {
            return i * sizeof(mouse_event_t);
        }

        ((mouse_event_t*)buffer)[i] = mouse->events[*offset];
        *offset = (*offset + 1) % MOUSE_MAX_EVENT;
    }

    return count;
}

static wait_queue_t* mouse_poll(file_t* file, poll_events_t events, poll_events_t* revents)
{
    mouse_t* mouse = file->inode->private;
    *revents = POLLIN & (mouse->writeIndex != file->pos);
    return &mouse->waitQueue;
}

static file_ops_t fileOps = {
    .read = mouse_read,
    .poll = mouse_poll,
};

static void mouse_inode_cleanup(inode_t* inode)
{
    mouse_t* mouse = inode->private;
    wait_queue_deinit(&mouse->waitQueue);
    heap_free(mouse);
}

static inode_ops_t inodeOps = {
    .cleanup = mouse_inode_cleanup,
};

mouse_t* mouse_new(const char* name)
{
    if (mouseDir.dentry == NULL)
    {
        if (sysfs_dir_init(&mouseDir, sysfs_get_default(), "mouse", NULL, NULL) == ERR)
        {
            return NULL;
        }
    }

    mouse_t* mouse = heap_alloc(sizeof(mouse_t), HEAP_NONE);
    mouse->writeIndex = 0;
    wait_queue_init(&mouse->waitQueue);
    lock_init(&mouse->lock);
    if (sysfs_file_init(&mouse->file, &mouseDir, name, &inodeOps, &fileOps, mouse) == ERR)
    {
        wait_queue_deinit(&mouse->waitQueue);
        heap_free(mouse);
        return NULL;
    }

    return mouse;
}

void mouse_free(mouse_t* mouse)
{
    sysfs_file_deinit(&mouse->file);
}

void mouse_push(mouse_t* mouse, mouse_buttons_t buttons, int64_t deltaX, int64_t deltaY)
{
    LOCK_SCOPE(&mouse->lock);
    mouse->events[mouse->writeIndex] = (mouse_event_t){
        .time = systime_uptime(),
        .buttons = buttons,
        .deltaX = deltaX,
        .deltaY = deltaY,
    };
    mouse->writeIndex = (mouse->writeIndex + 1) % MOUSE_MAX_EVENT;
    wait_unblock(&mouse->waitQueue, WAIT_ALL);
}
