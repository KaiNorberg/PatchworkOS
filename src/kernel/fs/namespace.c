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
#include <kernel/sync/rwlock.h>
#include <kernel/utils/map.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>

#define MOUNT_ID_STICKY UINT64_MAX

static map_key_t mount_key(mount_id_t parentId, dentry_id_t mountpointId, mode_t mode)
{
    struct
    {
        mount_id_t parentId;
        dentry_id_t mountpointId;
    } buffer;
    buffer.parentId = mode & MODE_STICKY ? MOUNT_ID_STICKY : parentId;
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

    return mount_key(mount->parent->id, mount->target->id, mount->mode);
}

static namespace_t* namespace_new(void)
{
    namespace_t* ns = malloc(sizeof(namespace_t));
    if (ns == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    list_entry_init(&ns->entry);
    list_init(&ns->children);
    ns->parent = NULL;
    list_init(&ns->stacks);
    map_init(&ns->mountMap);
    list_init(&ns->handles);
    rwlock_init(&ns->lock);

    return ns;
}

static void namespace_free(namespace_t* ns)
{
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
        while (!list_is_empty(&ns->children))
        {
            namespace_t* child = CONTAINER_OF(list_pop_front(&ns->children), namespace_t, entry);
            RWLOCK_WRITE_SCOPE(&child->lock);
            list_push_back(&ns->parent->children, &child->entry);
            child->parent = ns->parent;
        }
        list_remove(&ns->parent->children, &ns->entry);
        ns->parent = NULL;
    }
    else
    {
        while (!list_is_empty(&ns->children))
        {
            namespace_t* child = CONTAINER_OF(list_pop_front(&ns->children), namespace_t, entry);
            RWLOCK_WRITE_SCOPE(&child->lock);
            child->parent = NULL;
        }
    }

    free(ns);
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

    if (mount->parent == NULL || mount->mode & MODE_STICKY)
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

uint64_t namespace_handle_init(namespace_handle_t* handle, namespace_handle_t* source, namespace_handle_flags_t flags)
{
    if (handle == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    list_entry_init(&handle->entry);
    handle->ns = NULL;
    rwlock_init(&handle->lock);

    if (source == NULL)
    {
        namespace_t* ns = namespace_new();
        if (ns == NULL)
        {
            return ERR;
        }

        handle->ns = ns;
        list_push_back(&ns->handles, &handle->entry);
        return 0;
    }

    if (flags & NAMESPACE_HANDLE_EMPTY)
    {
        RWLOCK_READ_SCOPE(&source->lock);
        namespace_t* srcNs = source->ns;
        if (srcNs == NULL)
        {
            errno = EINVAL;
            return ERR;
        }

        namespace_t* ns = namespace_new();
        if (ns == NULL)
        {
            return ERR;
        }

        RWLOCK_WRITE_SCOPE(&srcNs->lock);

        ns->parent = srcNs;
        list_push_back(&srcNs->children, &ns->entry);

        handle->ns = ns;
        list_push_back(&ns->handles, &handle->entry);
        return 0;
    }

    if (flags & NAMESPACE_HANDLE_COPY)
    {
        RWLOCK_READ_SCOPE(&source->lock);
        namespace_t* srcNs = source->ns;
        if (srcNs == NULL)
        {
            errno = EINVAL;
            return ERR;
        }

        namespace_t* ns = namespace_new();
        if (ns == NULL)
        {
            return ERR;
        }

        RWLOCK_WRITE_SCOPE(&srcNs->lock);

        mount_stack_t* stack;
        LIST_FOR_EACH(stack, &srcNs->stacks, entry)
        {
            for (uint64_t i = 0; i < stack->count; i++)
            {
                if (stack->mounts[i]->mode & MODE_PRIVATE)
                {
                    continue;
                }

                map_key_t key = mount_key_from_mount(stack->mounts[i]);
                if (namespace_add(ns, stack->mounts[i], &key) == ERR)
                {
                    namespace_free(ns);
                    return ERR;
                }
            }
        }

        ns->parent = srcNs;
        list_push_back(&srcNs->children, &ns->entry);

        handle->ns = ns;
        list_push_back(&ns->handles, &handle->entry);
        return 0;
    }

    RWLOCK_READ_SCOPE(&source->lock);
    if (source->ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&source->ns->lock);
    handle->ns = source->ns;
    list_push_back(&source->ns->handles, &handle->entry);
    return 0;
}

void namespace_handle_deinit(namespace_handle_t* handle)
{
    namespace_handle_clear(handle);
}

void namespace_handle_clear(namespace_handle_t* handle)
{
    if (handle == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&handle->lock);

    if (handle->ns == NULL)
    {
        return;
    }

    namespace_t* ns = handle->ns;
    RWLOCK_WRITE_SCOPE(&ns->lock);

    list_remove(&ns->handles, &handle->entry);

    if (list_is_empty(&ns->handles))
    {
        namespace_free(ns);
    }

    handle->ns = NULL;
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

bool namespace_accessible(namespace_handle_t* handle, namespace_handle_t* other)
{
    if (handle == NULL || other == NULL)
    {
        return false;
    }

    if (handle == other)
    {
        return true;
    }

    RWLOCK_READ_SCOPE(&handle->lock);
    RWLOCK_READ_SCOPE(&other->lock);

    if (handle->ns == NULL || other->ns == NULL)
    {
        return false;
    }

    RWLOCK_READ_SCOPE(&handle->ns->lock);
    return namespace_is_descendant(handle->ns, other->ns);
}

bool namespace_traverse(namespace_handle_t* handle, path_t* path)
{
    if (handle == NULL || !PATH_IS_VALID(path))
    {
        return false;
    }

    // The mount count has race conditions, but the worst that can happen is a redundant lookup.
    if (atomic_load(&path->dentry->mountCount) == 0)
    {
        return false;
    }

    RWLOCK_READ_SCOPE(&handle->lock);
    namespace_t* ns = handle->ns;
    if (ns == NULL)
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

        map_key_t key = mount_key(path->mount->id, path->dentry->id, MODE_NONE);
        mount_stack_t* stack = CONTAINER_OF_SAFE(map_get(&ns->mountMap, &key), mount_stack_t, mapEntry);
        if (stack == NULL)
        {
            key = mount_key(path->mount->id, path->dentry->id, MODE_STICKY);
            stack = CONTAINER_OF_SAFE(map_get(&ns->mountMap, &key), mount_stack_t, mapEntry);

            if (stack == NULL)
            {
                return traversed;
            }
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

    static _Atomic(uint32_t) nextVirtId = ATOMIC_VAR_INIT(0);

    if (deviceName != NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    *out = (dev_t){.type = 0, .id = atomic_fetch_add_explicit(&nextVirtId, 1, memory_order_relaxed)};
    return 0;
}

mount_t* namespace_mount(namespace_handle_t* handle, path_t* target, const char* fsName, const char* deviceName,
    mode_t mode, void* private)
{
    if (handle == NULL || fsName == NULL)
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

    RWLOCK_READ_SCOPE(&handle->lock);
    namespace_t* ns = handle->ns;
    if (ns == NULL)
    {
        errno = EINVAL;
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

mount_t* namespace_bind(namespace_handle_t* handle, path_t* target, path_t* source, mode_t mode)
{
    if (handle == NULL || !PATH_IS_VALID(source))
    {
        errno = EINVAL;
        return NULL;
    }

    if (mode_check(&mode, source->mount->mode) == ERR)
    {
        return NULL;
    }

    RWLOCK_READ_SCOPE(&handle->lock);
    namespace_t* ns = handle->ns;
    if (ns == NULL)
    {
        errno = EINVAL;
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

void namespace_unmount(namespace_handle_t* handle, mount_t* mount, mode_t mode)
{
    if (handle == NULL || mount == NULL)
    {
        return;
    }

    RWLOCK_READ_SCOPE(&handle->lock);
    namespace_t* ns = handle->ns;
    if (ns == NULL)
    {
        return;
    }
    RWLOCK_WRITE_SCOPE(&ns->lock);

    map_key_t key = mount_key_from_mount(mount);
    namespace_remove(ns, mount, &key, mode);
}

void namespace_get_root(namespace_handle_t* handle, path_t* out)
{
    if (out == NULL)
    {
        path_set(out, NULL, NULL);
        return;
    }

    RWLOCK_READ_SCOPE(&handle->lock);
    namespace_t* ns = handle->ns;
    if (ns == NULL)
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

    path_t mountpath = cwd_get(&process->cwd, &process->ns);
    PATH_DEFER(&mountpath);

    if (path_walk(&mountpath, &mountname, &process->ns) == ERR)
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

    mount_t* mount =
        namespace_mount(&process->ns, &mountpath, fsName, device != NULL ? deviceName : NULL, mountname.mode, NULL);
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

    path_t mountpath = cwd_get(&process->cwd, &process->ns);
    PATH_DEFER(&mountpath);

    if (path_walk(&mountpath, &mountname, &process->ns) == ERR)
    {
        return ERR;
    }

    namespace_unmount(&process->ns, mountpath.mount, mountname.mode);
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

    path_t mountpath = cwd_get(&process->cwd, &process->ns);
    PATH_DEFER(&mountpath);

    if (path_walk(&mountpath, &mountname, &process->ns) == ERR)
    {
        return ERR;
    }

    file_t* sourceFile = file_table_get(&process->fileTable, source);
    if (sourceFile == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(sourceFile);

    mount_t* bind = namespace_bind(&process->ns, &mountpath, &sourceFile->path, mountname.mode);
    if (bind == NULL)
    {
        return ERR;
    }
    UNREF(bind);
    return 0;
}