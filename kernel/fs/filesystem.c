#include <kernel/fs/filesystem.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/binding.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/path.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/ref.h>

#include <kernel/cpu/regs.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/map.h>

static dentry_t* root = NULL;

static vnode_class_t rootClass = {
    .name = "fs root",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

static vnode_class_t dirClass = {
    .name = "fs dir",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

status_t filesystem_register(filesystem_t* fs)
{
    if (fs == NULL || fs->name == NULL || fs->clone == NULL)
    {
        return ERR(FS, INVAL);
    }

    if (root == NULL)
    {
        root = sysfs_dentry_new(NULL, "fs", &rootClass, NULL);
        if (root == NULL)
        {
            return ERR(FS, NOMEM);
        }
    }

    fs->internal.dir = sysfs_dentry_new(root, fs->name, &dirClass, fs);
    if (fs->internal.dir == NULL)
    {
        return ERR(FS, NOMEM);
    }

    fs->internal.clone = sysfs_dentry_new(fs->internal.dir, "clone", fs->clone, fs);
    if (fs->internal.clone == NULL)
    {
        UNREF(fs->internal.dir);
        fs->internal.dir = NULL;
        return ERR(FS, NOMEM);
    }

    return OK;
}

status_t filesystem_unregister(filesystem_t* fs)
{
    if (fs == NULL)
    {
        return ERR(FS, INVAL);
    }

    if (fs->internal.clone != NULL)
    {
        UNREF(fs->internal.clone);
        fs->internal.clone = NULL;
    }

    if (fs->internal.dir != NULL)
    {
        UNREF(fs->internal.dir);
        fs->internal.dir = NULL;
    }

    return OK;
}

file_volume_t volume_new(void)
{
    static _Atomic(file_volume_t) nextId = ATOMIC_VAR_INIT(1);
    return atomic_fetch_add(&nextId, 1);
}

bool options_next(const char** iter, char* buffer, size_t size, const char** key, char** value)
{
    while (*iter != NULL && **iter != '\0')
    {
        const char* start = *iter;
        const char* end = strchr(start, '&');
        size_t len = end != NULL ? (size_t)(end - start) : strlen(start);

        *iter = end != NULL ? end + 1 : NULL;

        if (len == 0)
        {
            continue;
        }
        if (len >= size)
        {
            continue;
        }

        memcpy(buffer, start, len);
        buffer[len] = '\0';

        *key = buffer;
        *value = strchr(*key, '=');

        if (*value != NULL)
        {
            *(*value)++ = '\0';
            return true;
        }
    }

    return false;
}
