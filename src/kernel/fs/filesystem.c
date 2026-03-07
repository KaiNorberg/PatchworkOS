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

status_t filesystem_register(filesystem_t* fs)
{
    if (fs == NULL || fs->name == NULL || fs->clone == NULL)
    {
        return ERR(FS, INVAL);
    }

    if (root == NULL)
    {
        root = devfs_dentry_new(NULL, "fs", &rootClass, fs);
        if (root == NULL)
        {
            return ERR(FS, NOMEM);
        }
    }

    fs->interna
}

bool options_next(const char** iter, char* buffer, size_t size, char** key, char** value)
{
    while (*iter != NULL && **iter != '\0')
    {
        const char* start = *iter;
        const char* end = strchr(start, ',');
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