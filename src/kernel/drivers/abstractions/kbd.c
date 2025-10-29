#include <kernel/drivers/abstractions/kbd.h>

#include <kernel/drivers/abstractions/kbd.h>
#include <kernel/fs/file.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

static dentry_t* kbdDir = NULL;

static atomic_uint64_t newId = ATOMIC_VAR_INIT(0);

static uint64_t kbd_events_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
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

static wait_queue_t* kbd_events_poll(file_t* file, poll_events_t* revents)
{
    kbd_t* kbd = file->inode->private;
    LOCK_SCOPE(&kbd->lock);
    if (kbd->writeIndex != file->pos)
    {
        *revents |= POLLIN;
    }
    return &kbd->waitQueue;
}

static file_ops_t eventsOps = {
    .read = kbd_events_read,
    .poll = kbd_events_poll,
};

static uint64_t kbd_name_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    kbd_t* kbd = file->inode->private;
    uint64_t nameLen = strnlen_s(kbd->name, MAX_NAME);
    if (*offset >= nameLen)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, kbd->name, nameLen);
}

static file_ops_t nameOps = {
    .read = kbd_name_read,
};

static void kbd_dir_cleanup(inode_t* inode)
{
    kbd_t* kbd = inode->private;
    wait_queue_deinit(&kbd->waitQueue);
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

    if (kbdDir == NULL)
    {
        kbdDir = sysfs_dir_new(NULL, "kbd", NULL, NULL);
        if (kbdDir == NULL)
        {
            return NULL;
        }
    }

    kbd_t* kbd = calloc(1, sizeof(kbd_t));
    if (kbd == NULL)
    {
        return NULL;
    }

    strncpy(kbd->name, name, MAX_NAME - 1);
    kbd->name[MAX_NAME - 1] = '\0';
    kbd->writeIndex = 0;
    kbd->mods = KBD_MOD_NONE;
    wait_queue_init(&kbd->waitQueue);
    lock_init(&kbd->lock);

    char id[MAX_NAME];
    if (snprintf(id, MAX_NAME, "%llu", atomic_fetch_add(&newId, 1)) < 0)
    {
        wait_queue_deinit(&kbd->waitQueue);
        free(kbd);
        return NULL;
    }

    kbd->dir = sysfs_dir_new(kbdDir, id, &dirInodeOps, kbd);
    if (kbd->dir == NULL)
    {
        wait_queue_deinit(&kbd->waitQueue);
        free(kbd);
        return NULL;
    }
    kbd->eventsFile = sysfs_file_new(kbd->dir, "events", NULL, &eventsOps, kbd);
    if (kbd->eventsFile == NULL)
    {
        DEREF(kbd->dir); // kbd will be freed in kbd_dir_cleanup
        return NULL;
    }
    kbd->nameFile = sysfs_file_new(kbd->dir, "name", NULL, &nameOps, kbd);
    if (kbd->nameFile == NULL)
    {
        DEREF(kbd->eventsFile);
        DEREF(kbd->dir);
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

    DEREF(kbd->dir);
    DEREF(kbd->eventsFile);
    DEREF(kbd->nameFile);
    // kbd will be freed in kbd_dir_cleanup
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
