#include <kernel/fs/namespace.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
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

static uint64_t namespace_add_local(namespace_t* ns, mount_t* mount, mount_flags_t flags, map_key_t* key)
{
    namespace_mount_t* nsMount = malloc(sizeof(namespace_mount_t));
    if (nsMount == NULL)
    {
        return ERR;
    }
    list_entry_init(&nsMount->entry);
    map_entry_init(&nsMount->mapEntry);
    nsMount->mount = NULL;
    nsMount->flags = flags;

    if (flags & MOUNT_OVERWRITE)
    {
        namespace_mount_t* existing = CONTAINER_OF_SAFE(map_get(&ns->mountMap, key), namespace_mount_t, mapEntry);

        if (map_replace(&ns->mountMap, key, &nsMount->mapEntry) == ERR)
        {
            free(nsMount);
            return ERR;
        }

        if (existing != NULL)
        {
            list_remove(&ns->mounts, &existing->entry);
            UNREF(existing->mount);
            free(existing);
        }
    }
    else
    {
        if (map_insert(&ns->mountMap, key, &nsMount->mapEntry) == ERR)
        {
            free(nsMount);
            return ERR;
        }
    }

    list_push_back(&ns->mounts, &nsMount->entry);

    nsMount->mount = REF(mount);
    return 0;
}

static uint64_t namespace_add(namespace_t* ns, mount_t* mount, mount_flags_t flags, map_key_t* key)
{
    if (namespace_add_local(ns, mount, flags, key) == ERR)
    {
        return ERR;
    }

    if (flags & MOUNT_PROPAGATE_CHILDREN)
    {
        namespace_t* child;
        LIST_FOR_EACH(child, &ns->children, entry)
        {
            RWLOCK_WRITE_SCOPE(&child->lock);

            if (namespace_add(child, mount, flags, key) == ERR)
            {
                continue; // Mount already exists in child namespace
            }
        }
    }

    if (flags & MOUNT_PROPAGATE_PARENT && ns->parent != NULL)
    {
        RWLOCK_WRITE_SCOPE(&ns->parent->lock);

        if (namespace_add(ns->parent, mount, flags, key) == ERR)
        {
            // Mount already exists in parent namespace
        }
    }

    return 0;
}

static void namespace_remove(namespace_t* ns, mount_t* mount, mount_flags_t flags)
{
    if (mount == ns->root)
    {
        return;
    }

    map_key_t key = mount_key(mount->parent->id, mount->target->id);
    namespace_mount_t* nsMount = CONTAINER_OF_SAFE(map_get(&ns->mountMap, &key), namespace_mount_t, mapEntry);

    if (nsMount == NULL || nsMount->mount != mount)
    {
        map_key_t stickyKey = mount_key(MOUNT_ID_STICKY, mount->target->id);
        namespace_mount_t* stickyMount =
            CONTAINER_OF_SAFE(map_get(&ns->mountMap, &stickyKey), namespace_mount_t, mapEntry);

        if (stickyMount != NULL && stickyMount->mount == mount)
        {
            nsMount = stickyMount;
        }
    }

    if (nsMount != NULL && nsMount->mount == mount)
    {
        map_remove(&ns->mountMap, &nsMount->mapEntry);
        list_remove(&ns->mounts, &nsMount->entry);
        UNREF(nsMount->mount);
        free(nsMount);
    }

    if (flags & MOUNT_PROPAGATE_CHILDREN)
    {
        namespace_t* child;
        LIST_FOR_EACH(child, &ns->children, entry)
        {
            namespace_remove(child, mount, flags);
        }
    }

    if (flags & MOUNT_PROPAGATE_PARENT && ns->parent != NULL)
    {
        namespace_remove(ns->parent, mount, flags);
    }
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
    list_init(&ns->mounts);
    map_init(&ns->mountMap);
    ns->root = NULL;
    list_init(&ns->members);
    rwlock_init(&ns->lock);

    return ns;
}

static void namespace_free(namespace_t* ns)
{
    if (ns->root != NULL)
    {
        UNREF(ns->root);
        ns->root = NULL;
    }

    while (!list_is_empty(&ns->mounts))
    {
        namespace_mount_t* nsMount = CONTAINER_OF(list_pop_first(&ns->mounts), namespace_mount_t, entry);

        map_remove(&ns->mountMap, &nsMount->mapEntry);
        UNREF(nsMount->mount);
        free(nsMount);
    }

    if (ns->parent != NULL)
    {
        RWLOCK_WRITE_SCOPE(&ns->parent->lock);
        while (!list_is_empty(&ns->children))
        {
            namespace_t* child = CONTAINER_OF(list_pop_first(&ns->children), namespace_t, entry);
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
            namespace_t* child = CONTAINER_OF(list_pop_first(&ns->children), namespace_t, entry);
            RWLOCK_WRITE_SCOPE(&child->lock);
            child->parent = NULL;
        }
    }

    free(ns);
}

uint64_t namespace_member_init(namespace_member_t* member, namespace_member_t* source, namespace_member_flags_t flags)
{
    if (member == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    list_entry_init(&member->entry);
    member->ns = NULL;
    rwlock_init(&member->lock);

    if (source == NULL)
    {
        namespace_t* ns = namespace_new();
        if (ns == NULL)
        {
            return ERR;
        }

        member->ns = ns;
        list_push_back(&ns->members, &member->entry);
        return 0;
    }

    if (flags & NAMESPACE_MEMBER_COPY)
    {
        RWLOCK_READ_SCOPE(&source->lock);
        namespace_t* srcNs = source->ns;
        if (srcNs == NULL)
        {
            errno = EINVAL;
            return ERR;
        }

        RWLOCK_WRITE_SCOPE(&srcNs->lock);

        namespace_t* ns = namespace_new();
        if (ns == NULL)
        {
            return ERR;
        }

        if (srcNs->root != NULL)
        {
            ns->root = REF(srcNs->root);
        }
        else
        {
            ns->root = NULL;
        }

        namespace_mount_t* srcMount;
        LIST_FOR_EACH(srcMount, &srcNs->mounts, entry)
        {
            if (srcMount->flags & MOUNT_NO_INHERIT)
            {
                continue;
            }

            map_key_t key;
            if (srcMount->flags & MOUNT_STICKY)
            {
                key = mount_key(MOUNT_ID_STICKY, srcMount->mount->target->id);
            }
            else
            {
                key = mount_key(srcMount->mount->parent->id, srcMount->mount->target->id);
            }

            if (namespace_add_local(ns, srcMount->mount, srcMount->flags, &key) == ERR)
            {
                namespace_free(ns);
                return ERR;
            }
        }

        ns->parent = srcNs;
        list_push_back(&srcNs->children, &ns->entry);

        member->ns = ns;
        list_push_back(&ns->members, &member->entry);
        return 0;
    }

    RWLOCK_READ_SCOPE(&source->lock);
    if (source->ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&source->ns->lock);
    member->ns = source->ns;
    list_push_back(&source->ns->members, &member->entry);
    return 0;
}

void namespace_member_deinit(namespace_member_t* member)
{
    namespace_member_clear(member);
}

void namespace_member_clear(namespace_member_t* member)
{
    if (member == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&member->lock);

    if (member->ns == NULL)
    {
        return;
    }

    namespace_t* ns = member->ns;
    RWLOCK_WRITE_SCOPE(&ns->lock);

    list_remove(&ns->members, &member->entry);

    if (list_is_empty(&ns->members))
    {
        namespace_free(ns);
    }

    member->ns = NULL;
}

void namespace_traverse(namespace_member_t* member, path_t* path)
{
    if (member == NULL || path == NULL || path->mount == NULL || path->dentry == NULL)
    {
        return;
    }

    // The mount count has race conditions, but the worst that can happen is a redundant lookup.
    if (atomic_load(&path->dentry->mountCount) == 0)
    {
        return;
    }

    RWLOCK_READ_SCOPE(&member->lock);
    namespace_t* ns = member->ns;
    if (ns == NULL)
    {
        return;
    }
    RWLOCK_READ_SCOPE(&ns->lock);

    map_key_t key = mount_key(path->mount->id, path->dentry->id);
    map_entry_t* entry = map_get(&ns->mountMap, &key);
    if (entry == NULL)
    {
        key = mount_key(MOUNT_ID_STICKY, path->dentry->id);
        entry = map_get(&ns->mountMap, &key);

        if (entry == NULL)
        {
            return;
        }
    }

    namespace_mount_t* nsMount = CONTAINER_OF(entry, namespace_mount_t, mapEntry);
    path_set(path, nsMount->mount, nsMount->mount->source);
}

mount_t* namespace_mount(namespace_member_t* member, path_t* target, const char* deviceName, const char* fsName,
    mount_flags_t flags, mode_t mode, void* private)
{
    if (member == NULL || deviceName == NULL || fsName == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    filesystem_t* fs = filesystem_get(fsName);
    if (fs == NULL)
    {
        errno = ENODEV;
        return NULL;
    }

    dentry_t* root = fs->mount(fs, deviceName, private);
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

    RWLOCK_READ_SCOPE(&member->lock);
    namespace_t* ns = member->ns;
    if (ns == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    RWLOCK_WRITE_SCOPE(&ns->lock);

    if (target == NULL)
    {
        if (ns->root != NULL)
        {
            errno = EBUSY;
            return NULL;
        }

        ns->root = mount_new(root->superblock, root, NULL, NULL, mode);
        if (ns->root == NULL)
        {
            return NULL;
        }

        LOG_INFO("mounted %s as root with %s\n", deviceName, fsName);
        return REF(ns->root);
    }

    if (ns->root == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    mount_t* mount = mount_new(root->superblock, root, target->dentry, target->mount, mode);
    if (mount == NULL)
    {
        return NULL;
    }

    map_key_t key;
    if (flags & MOUNT_STICKY)
    {
        key = mount_key(MOUNT_ID_STICKY, target->dentry->id);
    }
    else
    {
        key = mount_key(target->mount->id, target->dentry->id);
    }

    if (namespace_add(ns, mount, flags, &key) == ERR)
    {
        UNREF(mount);
        errno = ENOMEM;
        return NULL;
    }

    LOG_DEBUG("mounted %s with %s\n", deviceName, fsName);
    return mount;
}

mount_t* namespace_bind(namespace_member_t* member, dentry_t* source, path_t* target, mount_flags_t flags, mode_t mode)
{
    if (member == NULL || source == NULL || target == NULL || target->dentry == NULL || target->mount == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    mount_t* mount = mount_new(source->superblock, source, target->dentry, target->mount, mode);
    if (mount == NULL)
    {
        return NULL;
    }

    RWLOCK_READ_SCOPE(&member->lock);
    namespace_t* ns = member->ns;
    if (ns == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    RWLOCK_WRITE_SCOPE(&ns->lock);

    map_key_t key;
    if (flags & MOUNT_STICKY)
    {
        key = mount_key(MOUNT_ID_STICKY, target->dentry->id);
    }
    else
    {
        key = mount_key(target->mount->id, target->dentry->id);
    }

    if (namespace_add(ns, mount, flags, &key) == ERR)
    {
        UNREF(mount);
        errno = ENOMEM;
        return NULL;
    }

    return mount;
}

void namespace_unmount(namespace_member_t* member, mount_t* mount, mount_flags_t flags)
{
    if (member == NULL || mount == NULL || mount->source == NULL || mount->target == NULL)
    {
        return;
    }

    RWLOCK_READ_SCOPE(&member->lock);
    namespace_t* ns = member->ns;
    if (ns == NULL)
    {
        return;
    }
    RWLOCK_WRITE_SCOPE(&ns->lock);

    namespace_remove(ns, mount, flags);
}

uint64_t namespace_get_root_path(namespace_member_t* member, path_t* out)
{
    if (out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_READ_SCOPE(&member->lock);
    namespace_t* ns = member->ns;
    if (ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    RWLOCK_READ_SCOPE(&ns->lock);

    if (ns->root == NULL || ns->root->superblock == NULL || ns->root->source == NULL)
    {
        errno = ENOENT;
        return ERR;
    }

    path_set(out, ns->root, ns->root->source);
    return 0;
}

SYSCALL_DEFINE(SYS_BIND, uint64_t, fd_t source, const char* mountpoint, mount_flags_t flags)
{
    if (mountpoint == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t pathname;
    if (thread_copy_from_user_pathname(thread, &pathname, mountpoint) == ERR)
    {
        return ERR;
    }

    path_t mountPath = cwd_get(&process->cwd);
    PATH_DEFER(&mountPath);

    if (path_walk(&mountPath, &pathname, &process->ns) == ERR)
    {
        return ERR;
    }

    file_t* sourceFile = file_table_get(&process->fileTable, source);
    if (sourceFile == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(sourceFile);

    mount_t* bind = namespace_bind(&process->ns, sourceFile->path.dentry, &mountPath, flags, sourceFile->mode);
    if (bind == NULL)
    {
        return ERR;
    }
    UNREF(bind);
    return 0;
}