#include "kbd.h"
#include "lock.h"
#include "sys/kbd.h"
#include "sysfs.h"
#include "systime.h"

#include <stdlib.h>
#include <sys/math.h>

static uint64_t kbd_read(file_t* file, void* buffer, uint64_t count)
{
    kbd_t* kbd = file->sysobj->private;

    count = ROUND_DOWN(count, sizeof(kbd_event_t));
    for (uint64_t i = 0; i < count / sizeof(kbd_event_t); i++)
    {
        if (WAIT_BLOCK_LOCK(&kbd->waitQueue, &kbd->lock, file->pos != kbd->writeIndex) != WAIT_NORM)
        {
            lock_release(&kbd->lock);
            return i * sizeof(kbd_event_t);
        }

        ((kbd_event_t*)buffer)[i] = kbd->events[file->pos];
        file->pos = (file->pos + 1) % KBD_MAX_EVENT;

        lock_release(&kbd->lock);
    }

    return count;
}

static wait_queue_t* kbd_poll(file_t* file, poll_file_t* pollFile)
{
    kbd_t* kbd = file->sysobj->private;
    pollFile->occurred = POLL_READ & (kbd->writeIndex != file->pos);
    return &kbd->waitQueue;
}

SYSFS_STANDARD_SYSOBJ_OPEN_DEFINE(kbd_open, PATH_NONE,
    (file_ops_t){
        .read = kbd_read,
        .poll = kbd_poll,
    });

static void kbd_on_free(sysobj_t* sysobj)
{
    kbd_t* kbd = sysobj->private;
    free(kbd);
}

static sysobj_ops_t resOps = {
    .open = kbd_open,
    .onFree = kbd_on_free,
};

kbd_t* kbd_new(const char* name)
{
    kbd_t* kbd = malloc(sizeof(kbd_t));
    kbd->writeIndex = 0;
    kbd->mods = KBD_MOD_NONE;
    kbd->sysobj = sysobj_new("/kbd", name, &resOps, kbd);
    wait_queue_init(&kbd->waitQueue);
    lock_init(&kbd->lock);

    return kbd;
}

void kbd_free(kbd_t* kbd)
{
    sysobj_free(kbd->sysobj);
}

static void kbd_update_mod(kbd_t* kbd, kbd_event_type_t type, kbd_mods_t mod)
{
    if (type == KBD_PRESS)
    {
        kbd->mods |= mod;
    }
    else if (type == KBD_RELEASE)
    {
        kbd->mods &= ~mod;
    }
}

void kbd_push(kbd_t* kbd, kbd_event_type_t type, keycode_t code)
{
    LOCK_DEFER(&kbd->lock);

    switch (code)
    {
    case KBD_CAPS_LOCK:
        kbd_update_mod(kbd, type, KBD_MOD_CAPS);
        break;
    case KBD_LEFT_SHIFT:
    case KBD_RIGHT_SHIFT:
        kbd_update_mod(kbd, type, KBD_MOD_SHIFT);
        break;
    case KBD_LEFT_CTRL:
    case KBD_RIGHT_CTRL:
        kbd_update_mod(kbd, type, KBD_MOD_CTRL);
        break;
    case KBD_LEFT_ALT:
    case KBD_RIGHT_ALT:
        kbd_update_mod(kbd, type, KBD_MOD_ALT);
        break;
    case KBD_LEFT_SUPER:
    case KBD_RIGHT_SUPER:
        kbd_update_mod(kbd, type, KBD_MOD_SUPER);
        break;
    default:
        break;
    }

    kbd->events[kbd->writeIndex] = (kbd_event_t){
        .time = systime_uptime(),
        .code = code,
        .mods = kbd->mods,
        .type = type,
    };
    kbd->writeIndex = (kbd->writeIndex + 1) % KBD_MAX_EVENT;
    wait_unblock(&kbd->waitQueue, WAIT_ALL);
}
