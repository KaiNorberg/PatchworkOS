#include <kernel/fs/filesystem.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/cwd.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/key.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/sysfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/ref.h>

#include <kernel/cpu/regs.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>

static map_t filesystems = MAP_CREATE();
static rwlock_t filesystemsLock = RWLOCK_CREATE();

static map_key_t filesystem_key(const char* name)
{
    return map_key_string(name);
}

uint64_t filesystem_register(filesystem_t* fs)
{
    if (fs == NULL || strnlen_s(fs->name, MAX_NAME) > MAX_NAME)
    {
        errno = EINVAL;
        return ERR;
    }

    map_entry_init(&fs->mapEntry);
    list_init(&fs->superblocks);
    rwlock_init(&fs->lock);

    map_key_t key = filesystem_key(fs->name);

    RWLOCK_WRITE_SCOPE(&filesystemsLock);

    if (map_insert(&filesystems, &key, &fs->mapEntry) == ERR)
    {
        return ERR;
    }

    return 0;
}

void filesystem_unregister(filesystem_t* fs)
{
    if (fs == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&filesystemsLock);
    map_remove(&filesystems, &fs->mapEntry);

    while (!list_is_empty(&fs->superblocks))
    {
        list_pop_first(&fs->superblocks);
    }
}

filesystem_t* filesystem_get(const char* name)
{
    RWLOCK_READ_SCOPE(&filesystemsLock);

    map_key_t key = filesystem_key(name);
    return CONTAINER_OF_SAFE(map_get(&filesystems, &key), filesystem_t, mapEntry);
}
