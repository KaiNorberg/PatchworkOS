#include <kernel/fs/filesystem.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/key.h>
#include <kernel/fs/mount.h>
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

static dentry_t* dir = NULL;

static bool filesystem_cmp(map_entry_t* entry, const void* key)
{
    filesystem_t* fs = CONTAINER_OF(entry, filesystem_t, mapEntry);
    return strcmp(fs->name, (const char*)key) == 0;
}

static MAP_CREATE(fsMap, 64, filesystem_cmp);
static list_t filesystems = LIST_CREATE(filesystems);
static rwlock_t lock = RWLOCK_CREATE();

static status_t volume_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    volume_t* volume = frame->vnode->data;
    assert(volume != NULL);

    char info[MAX_PATH];
    int length = snprintf(info, sizeof(info), "id: %llu\n", volume->id);
    if (length < 0)
    {
        return ERR(FS, IMPL);
    }

    return irp_read_helper(irp, info, (size_t)length);
}

static void volume_cleanup(vnode_t* vnode)
{
    volume_t* volume = vnode->data;
    if (volume == NULL)
    {
        return;
    }

    UNREF(volume);
    vnode->data = NULL;
}

static vnode_class_t volumeClass = {
    .name = "volume file",
    .type = VNODE_REGULAR,
    .cleanup = volume_cleanup,
    .handlers =
        {
            [IRP_MJ_READ] = volume_read,
        },
};

static status_t filesystem_lookup(vnode_t* dir, dentry_t* dentry)
{
    filesystem_t* fs = dir->data;
    assert(fs != NULL);

    volume_id_t id;
    if (sscanf(dentry->name, "%llu", &id) != 1)
    {
        return INFO(FS, NEGATIVE);
    }

    RWLOCK_READ_SCOPE(&fs->lock);

    volume_t* volume;
    LIST_FOR_EACH(volume, &fs->volumes, entry)
    {
        if (volume->id != id)
        {
            continue;
        }

        vnode_t* vnode = vnode_new(dentry->volume, &volumeClass);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        vnode->data = REF(volume);
        dentry_make_positive(dentry, vnode);
        return OK;
    }

    return INFO(FS, NEGATIVE);
}

static status_t filesystem_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return OK;
    }

    filesystem_t* fs = dentry->vnode->data;
    assert(fs != NULL);

    RWLOCK_READ_SCOPE(&fs->lock);

    volume_t* volume;
    LIST_FOR_EACH(volume, &fs->volumes, entry)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        char name[MAX_NAME];
        snprintf(name, MAX_NAME, "%llu", volume->id);

        if (!ctx->emit(ctx, name, VNODE_REGULAR))
        {
            return OK;
        }
    }

    return OK;
}

static vnode_class_t fsClass = {
    .name = "fs dir",
    .type = VNODE_DIR,
    .lookup = filesystem_lookup,
    .iterate = filesystem_iterate,
};

static status_t filesystem_root_lookup(vnode_t* dir, dentry_t* dentry)
{
    UNUSED(dir);

    RWLOCK_READ_SCOPE(&lock);

    uint64_t hash = hash_buffer(dentry->name, strlen(dentry->name));
    map_entry_t* entry = map_find(&fsMap, dentry->name, hash);
    if (entry == NULL)
    {
        return OK;
    }
    filesystem_t* fs = CONTAINER_OF(entry, filesystem_t, mapEntry);

    vnode_t* vnode = vnode_new(dentry->volume, &fsClass);
    if (vnode == NULL)
    {
        return ERR(MEM, NOMEM);
    }
    UNREF_DEFER(vnode);
    vnode->data = fs;

    dentry_make_positive(dentry, vnode);
    return OK;
}

static status_t filesystem_root_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return OK;
    }

    RWLOCK_READ_SCOPE(&lock);

    filesystem_t* fs;
    LIST_FOR_EACH(fs, &filesystems, entry)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, fs->name, VNODE_DIR))
        {
            return OK;
        }
    }

    return OK;
}

static vnode_class_t rootClass = {
    .name = "fs root",
    .type = VNODE_DIR,
    .lookup = filesystem_root_lookup,
    .iterate = filesystem_root_iterate,
};

void filesystem_expose(void)
{
    RWLOCK_WRITE_SCOPE(&lock);

    if (dir != NULL)
    {
        LOG_ERR("filesystem already exposed\n");
        return;
    }

    dir = sysfs_dentry_new(NULL, "fs", &rootClass, NULL);
    if (dir == NULL)
    {
        panic(NULL, "failed to expose filesystem sysfs directory");
    }
}

status_t filesystem_register(filesystem_t* fs)
{
    if (fs == NULL || strnlen_s(fs->name, MAX_NAME) > MAX_NAME)
    {
        return ERR(FS, INVAL);
    }

    list_entry_init(&fs->entry);
    map_entry_init(&fs->mapEntry);
    list_init(&fs->volumes);
    rwlock_init(&fs->lock);

    uint64_t hash = hash_buffer(fs->name, strlen(fs->name));

    RWLOCK_WRITE_SCOPE(&lock);

    if (map_find(&fsMap, fs->name, hash) != NULL)
    {
        return ERR(FS, EXIST);
    }
    map_insert(&fsMap, &fs->mapEntry, hash);
    list_push_back(&filesystems, &fs->entry);
    return OK;
}

void filesystem_unregister(filesystem_t* fs)
{
    if (fs == NULL)
    {
        return;
    }

    uint64_t hash = hash_buffer(fs->name, strlen(fs->name));

    RWLOCK_WRITE_SCOPE(&lock);
    map_remove(&fsMap, &fs->mapEntry, hash);
    list_remove(&fs->entry);

    while (!list_is_empty(&fs->volumes))
    {
        list_pop_front(&fs->volumes);
    }
}

filesystem_t* filesystem_get_by_name(const char* name)
{
    RWLOCK_READ_SCOPE(&lock);

    uint64_t hash = hash_buffer(name, strlen(name));
    return CONTAINER_OF_SAFE(map_find(&fsMap, name, hash), filesystem_t, mapEntry);
}

filesystem_t* filesystem_get_by_path(const char* path, process_t* process)
{
    if (path == NULL || process == NULL)
    {
        return NULL;
    }

    pathname_t pathname;
    if (IS_ERR(pathname_init(&pathname, path)))
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

    if (IS_ERR(path_walk(&target, &pathname, ns)))
    {
        return NULL;
    }

    if (!DENTRY_IS_POSITIVE(target.dentry))
    {
        return NULL;
    }

    if (target.dentry->vnode->cls != &fsClass)
    {
        return NULL;
    }

    return target.dentry->vnode->data;
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