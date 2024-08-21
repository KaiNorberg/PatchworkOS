#include "mouse.h"
#include "lock.h"
#include "sys/mouse.h"
#include "sysfs.h"
#include "time.h"

#include <stdlib.h>
#include <sys/math.h>

static uint64_t mouse_read(file_t* file, void* buffer, uint64_t count)
{
    mouse_t* mouse = file->private;

    count = ROUND_DOWN(count, sizeof(mouse_event_t));
    for (uint64_t i = 0; i < count / sizeof(mouse_event_t); i++)
    {
        if (SCHED_BLOCK_LOCK(&mouse->blocker, &mouse->lock, file->pos != mouse->writeIndex) != BLOCK_NORM)
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

static uint64_t mouse_status(file_t* file, poll_file_t* pollFile)
{
    mouse_t* mouse = file->private;
    pollFile->occurred = POLL_READ & (mouse->writeIndex != file->pos);
    return 0;
}

static file_ops_t fileOps = {
    .read = mouse_read,
    .status = mouse_status,
};

static void mouse_delete(void* private)
{
    mouse_t* mouse = private;
    free(mouse);
}

mouse_t* mouse_new(const char* name)
{
    mouse_t* mouse = malloc(sizeof(mouse_t));
    mouse->writeIndex = 0;
    mouse->resource = sysfs_expose("/mouse", name, &fileOps, mouse, NULL, mouse_delete);
    blocker_init(&mouse->blocker);
    lock_init(&mouse->lock);

    return mouse;
}

void mouse_free(mouse_t* mouse)
{
    sysfs_hide(mouse->resource);
}

void mouse_push(mouse_t* mouse, mouse_buttons_t buttons, const point_t* delta)
{
    LOCK_GUARD(&mouse->lock);
    mouse->events[mouse->writeIndex] = (mouse_event_t){
        .time = time_uptime(),
        .buttons = buttons,
        .delta = *delta,
    };
    mouse->writeIndex = (mouse->writeIndex + 1) % MOUSE_MAX_EVENT;
    sched_unblock(&mouse->blocker);
}
