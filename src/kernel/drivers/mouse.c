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

static uint64_t mouse_read(file_t* file, void* buffer, uint64_t count)
{
    mouse_t* mouse = file->private;

    count = ROUND_DOWN(count, sizeof(mouse_event_t));
    for (uint64_t i = 0; i < count / sizeof(mouse_event_t); i++)
    {
        LOCK_DEFER(&mouse->lock);

        if (WAIT_BLOCK_LOCK(&mouse->waitQueue, &mouse->lock, file->pos != mouse->writeIndex) != WAIT_NORM)
        {
            return i * sizeof(mouse_event_t);
        }

        ((mouse_event_t*)buffer)[i] = mouse->events[file->pos];
        file->pos = (file->pos + 1) % MOUSE_MAX_EVENT;
    }

    return count;
}

static wait_queue_t* mouse_poll(file_t* file, poll_file_t* pollFile)
{
    mouse_t* mouse = file->private;
    pollFile->revents = POLL_READ & (mouse->writeIndex != file->pos);
    return &mouse->waitQueue;
}

SYSFS_STANDARD_OPS_DEFINE(mouseOps, PATH_NONE,
    (file_ops_t){
        .read = mouse_read,
        .poll = mouse_poll,
    });

mouse_t* mouse_new(const char* name)
{
    mouse_t* mouse = heap_alloc(sizeof(mouse_t), HEAP_NONE);
    mouse->writeIndex = 0;
    wait_queue_init(&mouse->waitQueue);
    lock_init(&mouse->lock);
    assert(sysobj_init_path(&mouse->sysobj, "/mouse", name, &mouseOps, mouse) != ERR);

    return mouse;
}

static void mouse_on_free(sysobj_t* sysobj)
{
    mouse_t* mouse = sysobj->private;
    heap_free(mouse);
}

void mouse_free(mouse_t* mouse)
{
    sysobj_deinit(&mouse->sysobj, mouse_on_free);
}

void mouse_push(mouse_t* mouse, mouse_buttons_t buttons, int64_t deltaX, int64_t deltaY)
{
    LOCK_DEFER(&mouse->lock);
    mouse->events[mouse->writeIndex] = (mouse_event_t){
        .time = systime_uptime(),
        .buttons = buttons,
        .deltaX = deltaX,
        .deltaY = deltaY,
    };
    mouse->writeIndex = (mouse->writeIndex + 1) % MOUSE_MAX_EVENT;
    wait_unblock(&mouse->waitQueue, WAIT_ALL);
}
