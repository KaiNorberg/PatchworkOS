#include "mouse.h"
#include "lock.h"
#include "sys/mouse.h"
#include "sysfs.h"
#include "systime.h"

#include <stdlib.h>
#include <sys/math.h>

static uint64_t mouse_read(file_t* file, void* buffer, uint64_t count)
{
    mouse_t* mouse = file->sysobj->private;

    count = ROUND_DOWN(count, sizeof(mouse_event_t));
    for (uint64_t i = 0; i < count / sizeof(mouse_event_t); i++)
    {
        if (WAITSYS_BLOCK_LOCK(&mouse->waitQueue, &mouse->lock, file->pos != mouse->writeIndex) != BLOCK_NORM)
        {
            lock_release(&mouse->lock);
            return i * sizeof(mouse_event_t);
        }

        ((mouse_event_t*)buffer)[i] = mouse->events[file->pos];
        file->pos = (file->pos + 1) % MOUSE_MAX_EVENT;

        lock_release(&mouse->lock);
    }

    return count;
}

static wait_queue_t* mouse_poll(file_t* file, poll_file_t* pollFile)
{
    mouse_t* mouse = file->sysobj->private;
    pollFile->occurred = POLL_READ & (mouse->writeIndex != file->pos);
    return &mouse->waitQueue;
}

static file_ops_t fileOps = {
    .read = mouse_read,
    .poll = mouse_poll,
};

SYSFS_STANDARD_SYSOBJ_OPEN_DEFINE(mouse_open, &fileOps);

static void mouse_on_free(sysobj_t* sysobj)
{
    mouse_t* mouse = sysobj->private;
    free(mouse);
}

static sysobj_ops_t resOps = {
    .open = mouse_open,
    .onFree = mouse_on_free,
};

mouse_t* mouse_new(const char* name)
{
    mouse_t* mouse = malloc(sizeof(mouse_t));
    mouse->writeIndex = 0;
    mouse->sysobj = sysobj_new("/mouse", name, &resOps, mouse);
    wait_queue_init(&mouse->waitQueue);
    lock_init(&mouse->lock);

    return mouse;
}

void mouse_free(mouse_t* mouse)
{
    sysobj_free(mouse->sysobj);
}

void mouse_push(mouse_t* mouse, mouse_buttons_t buttons, const point_t* delta)
{
    LOCK_DEFER(&mouse->lock);
    mouse->events[mouse->writeIndex] = (mouse_event_t){
        .time = systime_uptime(),
        .buttons = buttons,
        .delta = *delta,
    };
    mouse->writeIndex = (mouse->writeIndex + 1) % MOUSE_MAX_EVENT;
    waitsys_unblock(&mouse->waitQueue, WAITSYS_ALL);
}
