#include "kbd.h"

#include "drivers/helpers/kbd.h"
#include "fs/file.h"
#include "fs/sysfs.h"
#include "mem/heap.h"
#include "sched/timer.h"
#include "sync/lock.h"

#include <errno.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

static sysfs_dir_t kbdDir = {0};

static uint64_t kbd_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    kbd_t* kbd = file->inode->private;

    count = ROUND_DOWN(count, sizeof(kbd_event_t));
    for (uint64_t i = 0; i < count / sizeof(kbd_event_t); i++)
    {
        LOCK_SCOPE(&kbd->lock);

        if (WAIT_BLOCK_LOCK(&kbd->waitQueue, &kbd->lock, *offset != kbd->writeIndex) == ERR)
        {
            return i * sizeof(kbd_event_t);
        }

        ((kbd_event_t*)buffer)[i] = kbd->events[*offset];
        *offset = (*offset + 1) % KBD_MAX_EVENT;
    }

    return count;
}

static wait_queue_t* kbd_poll(file_t* file, poll_events_t* revents)
{
    kbd_t* kbd = file->inode->private;
    LOCK_SCOPE(&kbd->lock);
    if (kbd->writeIndex != file->pos)
    {
        *revents |= POLLIN;
    }
    return &kbd->waitQueue;
}

static file_ops_t fileOps = {
    .read = kbd_read,
    .poll = kbd_poll,
};

static void kbd_inode_cleanup(inode_t* inode)
{
    kbd_t* kbd = inode->private;
    wait_queue_deinit(&kbd->waitQueue);
    heap_free(kbd);
}

static inode_ops_t inodeOps = {
    .cleanup = kbd_inode_cleanup,
};

kbd_t* kbd_new(const char* name)
{
    if (kbdDir.dentry == NULL)
    {
        if (sysfs_dir_init(&kbdDir, sysfs_get_dev(), "kbd", NULL, NULL) == ERR)
        {
            return NULL;
        }
    }

    kbd_t* kbd = heap_alloc(sizeof(kbd_t), HEAP_NONE);
    kbd->writeIndex = 0;
    kbd->mods = KBD_MOD_NONE;
    wait_queue_init(&kbd->waitQueue);
    lock_init(&kbd->lock);
    if (sysfs_file_init(&kbd->file, &kbdDir, name, &inodeOps, &fileOps, kbd) == ERR)
    {
        wait_queue_deinit(&kbd->waitQueue);
        heap_free(kbd);
        return NULL;
    }

    return kbd;
}

void kbd_free(kbd_t* kbd)
{
    sysfs_file_deinit(&kbd->file);
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
    LOCK_SCOPE(&kbd->lock);

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
        .time = timer_uptime(),
        .code = code,
        .mods = kbd->mods,
        .type = type,
    };
    kbd->writeIndex = (kbd->writeIndex + 1) % KBD_MAX_EVENT;
    wait_unblock(&kbd->waitQueue, WAIT_ALL, EOK);
}
