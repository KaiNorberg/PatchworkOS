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
#include <kernel/fs/vfs.h>
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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>

static dentry_t* dir = NULL;

static map_t fsMap = MAP_CREATE();
static list_t filesystems = LIST_CREATE(filesystems);
static rwlock_t lock = RWLOCK_CREATE();

static map_key_t filesystem_key(const char* name)
{
    return map_key_string(name);
}

static size_t superblock_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    superblock_t* sb = file->inode->private;
    assert(sb != NULL);

    char info[MAX_PATH];
    int len = snprintf(info, sizeof(info), "id: %llu\nblock_size: %llu\nmax_file_size: %llu\n", sb->id, sb->blockSize,
        sb->maxFileSize);
    if (len < 0)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, info, len);
}

static void superblock_cleanup(inode_t* inode)
{
    superblock_t* sb = inode->private;
    if (sb == NULL)
    {
        return;
    }

    UNREF(sb);
    inode->private = NULL;
}

static file_ops_t sbFileOps = {
    .read = superblock_read,
};

static inode_ops_t sbInodeOps = {
    .cleanup = superblock_cleanup,
};

static uint64_t filesystem_lookup(inode_t* dir, dentry_t* dentry)
{
    filesystem_t* fs = dir->private;
    assert(fs != NULL);

    sbid_t id;
    if (sscanf(dentry->name, "%llu", &id) != 1)
    {
        return 0;
    }

    RWLOCK_READ_SCOPE(&fs->lock);

    superblock_t* sb;
    LIST_FOR_EACH(sb, &fs->superblocks, entry)
    {
        if (sb->id != id)
        {
            continue;
        }

        inode_t* inode =
            inode_new(dentry->superblock, ino_gen(dir->number, dentry->name), INODE_FILE, NULL, &sbFileOps);
        if (inode == NULL)
        {
            return ERR;
        }
        inode->private = REF(sb);
        dentry_make_positive(dentry, inode);
        return 0;
    }

    return 0;
}

static uint64_t filesystem_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return 0;
    }

    filesystem_t* fs = dentry->inode->private;
    assert(fs != NULL);

    RWLOCK_READ_SCOPE(&fs->lock);

    superblock_t* sb;
    LIST_FOR_EACH(sb, &fs->superblocks, entry)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        char name[MAX_NAME];
        snprintf(name, MAX_NAME, "%llu", sb->id);

        if (!ctx->emit(ctx, name, ino_gen(dentry->inode->number, name), INODE_FILE))
        {
            return 0;
        }
    }

    return 0;
}

static inode_ops_t fsInodeOps = {
    .lookup = filesystem_lookup,
};

static dentry_ops_t fsDentryOps = {
    .iterate = filesystem_iterate,
};

static uint64_t filesystem_dir_lookup(inode_t* dir, dentry_t* dentry)
{
    RWLOCK_READ_SCOPE(&lock);

    map_key_t key = filesystem_key(dentry->name);
    filesystem_t* fs = CONTAINER_OF_SAFE(map_get(&fsMap, &key), filesystem_t, mapEntry);
    if (fs == NULL)
    {
        return 0;
    }

    inode_t* inode = inode_new(dentry->superblock, ino_gen(dir->number, fs->name), INODE_DIR, &fsInodeOps, NULL);
    if (inode == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(inode);
    inode->private = fs;

    dentry->ops = &fsDentryOps;
    dentry_make_positive(dentry, inode);
    return 0;
}

static uint64_t filesystem_dir_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return 0;
    }

    RWLOCK_READ_SCOPE(&lock);

    filesystem_t* fs;
    LIST_FOR_EACH(fs, &filesystems, entry)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, fs->name, ino_gen(dentry->inode->number, fs->name), INODE_DIR))
        {
            return 0;
        }
    }

    return 0;
}

static inode_ops_t dirInodeOps = {
    .lookup = filesystem_dir_lookup,
};

static dentry_ops_t dirDentryOps = {
    .iterate = filesystem_dir_iterate,
};

void filesystem_expose(void)
{
    RWLOCK_WRITE_SCOPE(&lock);

    if (dir != NULL)
    {
        LOG_ERR("filesystem already exposed\n");
        return;
    }

    dir = sysfs_dir_new(NULL, "fs", &dirInodeOps, NULL);
    if (dir == NULL)
    {
        panic(NULL, "failed to expose filesystem sysfs directory");
    }
    dir->ops = &dirDentryOps;
}

uint64_t filesystem_register(filesystem_t* fs)
{
    if (fs == NULL || strnlen_s(fs->name, MAX_NAME) > MAX_NAME)
    {
        errno = EINVAL;
        return ERR;
    }

    list_entry_init(&fs->entry);
    map_entry_init(&fs->mapEntry);
    list_init(&fs->superblocks);
    rwlock_init(&fs->lock);

    map_key_t key = filesystem_key(fs->name);

    RWLOCK_WRITE_SCOPE(&lock);

    if (map_insert(&fsMap, &key, &fs->mapEntry) == ERR)
    {
        return ERR;
    }
    list_push_back(&filesystems, &fs->entry);

    return 0;
}

void filesystem_unregister(filesystem_t* fs)
{
    if (fs == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&lock);
    map_remove(&fsMap, &fs->mapEntry);
    list_remove(&filesystems, &fs->entry);

    while (!list_is_empty(&fs->superblocks))
    {
        list_pop_front(&fs->superblocks);
    }
}

filesystem_t* filesystem_get_by_name(const char* name)
{
    RWLOCK_READ_SCOPE(&lock);

    map_key_t key = filesystem_key(name);
    return CONTAINER_OF_SAFE(map_get(&fsMap, &key), filesystem_t, mapEntry);
}

filesystem_t* filesystem_get_by_path(const char* path, process_t* process)
{
    if (path == NULL || process == NULL)
    {
        return NULL;
    }

    pathname_t pathname;
    if (pathname_init(&pathname, path) == ERR)
    {
        return NULL;
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(ns);

    path_t target = cwd_get(&process->cwd, ns);
    PATH_DEFER(&target);

    if (path_walk(&target, &pathname, ns) == ERR)
    {
        return NULL;
    }

    if (!DENTRY_IS_POSITIVE(target.dentry))
    {
        errno = ENOENT;
        return NULL;
    }

    if (target.dentry->ops != &fsDentryOps)
    {
        errno = EINVAL;
        return NULL;
    }

    return target.dentry->inode->private;
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