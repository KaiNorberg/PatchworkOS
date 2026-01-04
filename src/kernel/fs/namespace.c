#include <kernel/fs/namespace.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/proc/process.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/map.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>

static map_key_t mount_key(mount_id_t parentId, dentry_id_t mountpointId)
{
    struct
    {
        mount_id_t parentId;
        dentry_id_t mountpointId;
    } buffer;
    buffer.parentId = parentId;
    buffer.mountpointId = mountpointId;

    return map_key_buffer(&buffer, sizeof(buffer));
}

static map_key_t root_key(void)
{
    struct
    {
        mount_id_t parentId;
        dentry_id_t mountpointId;
    } buffer = {.parentId = UINT64_MAX, .mountpointId = UINT64_MAX};

    return map_key_buffer(&buffer, sizeof(buffer));
}

static map_key_t mount_key_from_mount(mount_t* mount)
{
    if (mount->parent == NULL)
    {
        return root_key();
    }

    return mount_key(mount->parent->id, mount->target->id);
}

static uint64_t namespace_add(namespace_t* ns, mount_t* mount, const map_key_t* key)
{
    mount_stack_t* stack = CONTAINER_OF_SAFE(map_get(&ns->mountMap, key), mount_stack_t, mapEntry);
    if (stack == NULL)
    {
        stack = malloc(sizeof(mount_stack_t));
        if (stack == NULL)
        {
            errno = ENOMEM;
            return ERR;
        }

        list_entry_init(&stack->entry);
        map_entry_init(&stack->mapEntry);
        memset((void*)stack->mounts, 0, sizeof(stack->mounts));
        stack->count = 0;

        if (map_insert(&ns->mountMap, key, &stack->mapEntry) == ERR)
        {
            free(stack);
            return ERR;
        }
        list_push_back(&ns->stacks, &stack->entry);
    }

    if (stack->count >= ARRAY_SIZE(stack->mounts))
    {
        errno = ENOMEM;
        return ERR;
    }

    if (mount->parent == NULL)
    {
        stack->mounts[stack->count] = REF(mount);
        stack->count++;
    }
    else
    {
        map_key_t parentKey = mount_key_from_mount(mount->parent);
        mount_stack_t* parentStack = CONTAINER_OF_SAFE(map_get(&ns->mountMap, &parentKey), mount_stack_t, mapEntry);
        if (parentStack != NULL)
        {
            stack->mounts[stack->count] = REF(mount);
            stack->count++;
        }
    }

    if (mount->mode & MODE_PROPAGATE)
    {
        namespace_t* child;
        LIST_FOR_EACH(child, &ns->children, entry)
        {
            RWLOCK_WRITE_SCOPE(&child->lock);

            if (namespace_add(child, mount, key) == ERR)
            {
                return ERR;
            }
        }
    }

    return 0;
}

static void namespace_remove(namespace_t* ns, mount_t* mount, map_key_t* key, mode_t mode)
{
    if (mount->parent == NULL)
    {
        return;
    }

    if (mount->mode & MODE_LOCKED)
    {
        return;
    }

    mount_stack_t* stack = CONTAINER_OF_SAFE(map_get(&ns->mountMap, key), mount_stack_t, mapEntry);
    if (stack != NULL)
    {
        for (uint64_t i = 0; i < stack->count; i++)
        {
            if (stack->mounts[i] != mount)
            {
                continue;
            }

            UNREF(stack->mounts[i]);
            stack->mounts[i] = NULL;

            memmove(&stack->mounts[i], &stack->mounts[i + 1], (stack->count - i - 1) * sizeof(mount_t*));
            stack->count--;
            break;
        }

        if (stack->count == 0)
        {
            list_remove(&ns->stacks, &stack->entry);
            map_remove(&ns->mountMap, &stack->mapEntry);
            free(stack);
        }
    }

    if (mode & MODE_PROPAGATE)
    {
        namespace_t* child;
        LIST_FOR_EACH(child, &ns->children, entry)
        {
            RWLOCK_WRITE_SCOPE(&child->lock);

            namespace_remove(child, mount, key, mode);
        }
    }
}

static void namespace_free(namespace_t* ns)
{
    if (ns == NULL)
    {
        return;
    }

    rwlock_write_acquire(&ns->lock);

    while (!list_is_empty(&ns->stacks))
    {
        mount_stack_t* stack = CONTAINER_OF(list_pop_front(&ns->stacks), mount_stack_t, entry);

        for (uint64_t i = 0; i < stack->count; i++)
        {
            UNREF(stack->mounts[i]);
            stack->mounts[i] = NULL;
        }

        map_remove(&ns->mountMap, &stack->mapEntry);
        free(stack);
    }

    if (ns->parent != NULL)
    {
        RWLOCK_WRITE_SCOPE(&ns->parent->lock);
        list_remove(&ns->parent->children, &ns->entry);
        UNREF(ns->parent);
        ns->parent = NULL;
    }

    map_deinit(&ns->mountMap);

    rwlock_write_release(&ns->lock);

    free(ns);
}

namespace_t* namespace_new(namespace_t* parent)
{
    namespace_t* ns = malloc(sizeof(namespace_t));
    if (ns == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    ref_init(&ns->ref, namespace_free);
    list_entry_init(&ns->entry);
    list_init(&ns->children);
    ns->parent = NULL;
    list_init(&ns->stacks);
    map_init(&ns->mountMap);
    rwlock_init(&ns->lock);

    if (parent != NULL)
    {
        RWLOCK_WRITE_SCOPE(&parent->lock);
        ns->parent = REF(parent);
        list_push_back(&parent->children, &ns->entry);
    }

    return ns;
}

uint64_t namespace_copy(namespace_t* dest, namespace_t* src)
{
    if (dest == NULL || src == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&dest->lock);
    RWLOCK_WRITE_SCOPE(&src->lock);

    mount_stack_t* stack;
    LIST_FOR_EACH(stack, &src->stacks, entry)
    {
        for (uint64_t i = 0; i < stack->count; i++)
        {
            if (stack->mounts[i]->mode & MODE_PRIVATE)
            {
                continue;
            }

            map_key_t key = mount_key_from_mount(stack->mounts[i]);
            if (namespace_add(dest, stack->mounts[i], &key) == ERR)
            {
                return ERR;
            }
        }
    }

    return 0;
}

static bool namespace_is_descendant(namespace_t* ancestor, namespace_t* descendant)
{
    // To prevent deadlocks we cant do a search starting from the child, we must always acquire looks from the top down.
    // So this is a bit inefficient.

    if (ancestor == descendant)
    {
        return true;
    }

    namespace_t* child;
    LIST_FOR_EACH(child, &ancestor->children, entry)
    {
        RWLOCK_READ_SCOPE(&child->lock);
        if (namespace_is_descendant(child, descendant))
        {
            return true;
        }
    }

    return false;
}

bool namespace_accessible(namespace_t* ns, namespace_t* other)
{
    if (ns == NULL || other == NULL)
    {
        return false;
    }

    RWLOCK_READ_SCOPE(&ns->lock);
    return namespace_is_descendant(ns, other);
}

bool namespace_traverse(namespace_t* ns, path_t* path)
{
    if (ns == NULL || !PATH_IS_VALID(path))
    {
        return false;
    }

    // The mount count has race conditions, but the worst that can happen is a redundant lookup.
    if (atomic_load(&path->dentry->mountCount) == 0)
    {
        return false;
    }

    RWLOCK_READ_SCOPE(&ns->lock);

    bool traversed = false;
    for (uint64_t i = 0; i < NAMESPACE_MAX_TRAVERSE; i++)
    {
        if (atomic_load(&path->dentry->mountCount) == 0)
        {
            return traversed;
        }

        map_key_t key = mount_key(path->mount->id, path->dentry->id);
        mount_stack_t* stack = CONTAINER_OF_SAFE(map_get(&ns->mountMap, &key), mount_stack_t, mapEntry);
        if (stack == NULL)
        {
            return traversed;
        }

        assert(stack->count > 0);
        mount_t* mnt = stack->mounts[stack->count - 1];

        path_set(path, mnt, mnt->source);
        traversed = true;
    }

    return traversed;
}

static uint64_t namespace_find_device(const char* deviceName, dev_t* out)
{
    /// @todo Implement physical device lookup.

    static _Atomic(uint32_t) nextVirtId = ATOMIC_VAR_INIT(1);

    if (deviceName != NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    *out = (dev_t){.type = 0, .id = atomic_fetch_add_explicit(&nextVirtId, 1, memory_order_relaxed)};
    return 0;
}

mount_t* namespace_mount(namespace_t* ns, path_t* target, const char* fsName, const char* deviceName, mode_t mode,
    void* private)
{
    if (ns == NULL || fsName == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    filesystem_t* fs = filesystem_get(fsName);
    if (fs == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    dev_t dev;
    if (namespace_find_device(deviceName, &dev) == ERR)
    {
        return NULL;
    }

    dentry_t* root = fs->mount(fs, dev, private);
    if (root == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(root);

    if (root->superblock->root != root)
    {
        errno = EIO;
        return NULL;
    }

    RWLOCK_WRITE_SCOPE(&ns->lock);

    mount_t* mount = mount_new(root->superblock, root, target != NULL ? target->dentry : NULL,
        target != NULL ? target->mount : NULL, mode);
    if (mount == NULL)
    {
        return NULL;
    }

    map_key_t key = mount_key_from_mount(mount);
    if (namespace_add(ns, mount, &key) == ERR)
    {
        UNREF(mount);
        return NULL;
    }

    return mount;
}

mount_t* namespace_bind(namespace_t* ns, path_t* target, path_t* source, mode_t mode)
{
    if (ns == NULL || !PATH_IS_VALID(source))
    {
        errno = EINVAL;
        return NULL;
    }

    if (mode_check(&mode, source->mount->mode) == ERR)
    {
        return NULL;
    }

    RWLOCK_WRITE_SCOPE(&ns->lock);

    mount_t* mount = mount_new(source->dentry->superblock, source->dentry, target != NULL ? target->dentry : NULL,
        target != NULL ? target->mount : NULL, mode);
    if (mount == NULL)
    {
        return NULL;
    }

    map_key_t key = mount_key_from_mount(mount);
    if (namespace_add(ns, mount, &key) == ERR)
    {
        UNREF(mount);
        errno = ENOMEM;
        return NULL;
    }

    return mount;
}

void namespace_unmount(namespace_t* ns, mount_t* mount, mode_t mode)
{
    if (ns == NULL || mount == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&ns->lock);

    map_key_t key = mount_key_from_mount(mount);
    namespace_remove(ns, mount, &key, mode);
}

void namespace_get_root(namespace_t* ns, path_t* out)
{
    if (ns == NULL || out == NULL)
    {
        path_set(out, NULL, NULL);
        return;
    }

    RWLOCK_READ_SCOPE(&ns->lock);

    map_key_t key = root_key();
    mount_stack_t* stack = CONTAINER_OF_SAFE(map_get(&ns->mountMap, &key), mount_stack_t, mapEntry);
    if (stack == NULL)
    {
        path_set(out, NULL, NULL);
        return;
    }

    assert(stack->count > 0);
    mount_t* mnt = stack->mounts[stack->count - 1];

    path_set(out, mnt, mnt->source);
}

SYSCALL_DEFINE(SYS_MOUNT, uint64_t, const char* mountpoint, const char* fs, const char* device)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t mountname;
    if (thread_copy_from_user_pathname(thread, &mountname, mountpoint) == ERR)
    {
        return ERR;
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(ns);

    path_t mountpath = cwd_get(&process->cwd, ns);
    PATH_DEFER(&mountpath);

    if (path_walk(&mountpath, &mountname, ns) == ERR)
    {
        return ERR;
    }

    char deviceName[MAX_NAME];
    if (device != NULL && thread_copy_from_user_string(thread, deviceName, device, MAX_NAME) == ERR)
    {
        return ERR;
    }

    char fsName[MAX_NAME];
    if (thread_copy_from_user_string(thread, fsName, fs, MAX_NAME) == ERR)
    {
        return ERR;
    }

    mount_t* mount = namespace_mount(ns, &mountpath, fsName, device != NULL ? deviceName : NULL, mountname.mode, NULL);
    if (mount == NULL)
    {
        return ERR;
    }
    UNREF(mount);
    return 0;
}

SYSCALL_DEFINE(SYS_UNMOUNT, uint64_t, const char* mountpoint)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t mountname;
    if (thread_copy_from_user_pathname(thread, &mountname, mountpoint) == ERR)
    {
        return ERR;
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(ns);

    path_t mountpath = cwd_get(&process->cwd, ns);
    PATH_DEFER(&mountpath);

    if (path_walk(&mountpath, &mountname, ns) == ERR)
    {
        return ERR;
    }

    namespace_unmount(ns, mountpath.mount, mountname.mode);
    return 0;
}

SYSCALL_DEFINE(SYS_BIND, uint64_t, const char* mountpoint, fd_t source)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t mountname;
    if (thread_copy_from_user_pathname(thread, &mountname, mountpoint) == ERR)
    {
        return ERR;
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(ns);

    path_t mountpath = cwd_get(&process->cwd, ns);
    PATH_DEFER(&mountpath);

    if (path_walk(&mountpath, &mountname, ns) == ERR)
    {
        return ERR;
    }

    file_t* sourceFile = file_table_get(&process->fileTable, source);
    if (sourceFile == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(sourceFile);

    mount_t* bind = namespace_bind(ns, &mountpath, &sourceFile->path, mountname.mode);
    if (bind == NULL)
    {
        return ERR;
    }
    UNREF(bind);
    return 0;
}