#include <kernel/fs/namespace.h>

#include <kernel/cpu/syscalls.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/proc/process.h>
#include <kernel/sched/thread.h>
#include <kernel/utils/map.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/io.h>

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

static uint64_t namespace_add(namespace_t* ns, mount_t* mount, mount_flags_t flags, map_key_t* key)
{
    namespace_mount_t* nsMount = malloc(sizeof(namespace_mount_t));
    if (nsMount == NULL)
    {
        return ERR;
    }
    list_entry_init(&nsMount->entry);
    map_entry_init(&nsMount->mapEntry);
    nsMount->mount = NULL;

    rwlock_write_acquire(&ns->lock);
    
    if (flags & MOUNT_OVERWRITE)
    {
        namespace_mount_t* existing = CONTAINER_OF_SAFE(map_get(&ns->mountMap, key), namespace_mount_t, mapEntry);

        if (map_replace(&ns->mountMap, key, &nsMount->mapEntry) == ERR)
        {
            rwlock_write_release(&ns->lock);
            return ERR;
        }

        if (existing != NULL)
        {
            list_remove(&ns->mounts, &existing->entry);
            DEREF(existing->mount);
            free(existing);
        }
    }
    else
    {
        if (map_insert(&ns->mountMap, key, &nsMount->mapEntry) == ERR)
        {
            rwlock_write_release(&ns->lock);
            return ERR;
        }
    }
    
    list_push_back(&ns->mounts, &nsMount->entry);
    
    nsMount->mount = REF(mount);

    if (flags & MOUNT_PROPAGATE_CHILDREN)
    {
        namespace_t* child;
        LIST_FOR_EACH(child, &ns->children, entry)
        {
            if (namespace_add(child, mount, flags, key) == ERR)
            {
                continue; // Mount already exists in child namespace
            }
        }
    }

    if (flags & MOUNT_PROPAGATE_PARENT && ns->parent != NULL)
    {
        if (namespace_add(ns->parent, mount, flags, key) == ERR)
        {
            // Mount already exists in parent namespace
        }
    }

    rwlock_write_release(&ns->lock);

    return 0;
}

uint64_t namespace_init(namespace_t* ns, namespace_t* parent)
{
    list_entry_init(&ns->entry);
    list_init(&ns->children);
    ns->parent = parent;
    list_init(&ns->mounts);
    map_init(&ns->mountMap);
    ns->root = parent != NULL ? REF(parent->root) : NULL;
    rwlock_init(&ns->lock);

    if (parent != NULL)
    {
        RWLOCK_WRITE_SCOPE(&parent->lock);
        list_push_back(&parent->children, &ns->entry);

        namespace_mount_t* nsMount;
        LIST_FOR_EACH(nsMount, &parent->mounts, entry)
        {
            map_key_t key = mount_key(nsMount->mount->parent->id, nsMount->mount->mountpoint->id);
            if (namespace_add(ns, nsMount->mount, MOUNT_PROPAGATE_CHILDREN, &key) == ERR)
            {
                if (errno == ENOMEM)
                {
                    namespace_deinit(ns);
                    return ERR;
                }
            }
        }

    }

    return 0;
}

void namespace_deinit(namespace_t* ns)
{
    if (ns == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&ns->lock);

    if (ns->root != NULL)
    {
        DEREF(ns->root);
        ns->root = NULL;
    }

    while (!list_is_empty(&ns->mounts))
    {
        namespace_mount_t* nsMount = CONTAINER_OF(list_pop_first(&ns->mounts), namespace_mount_t, entry);

        map_remove(&ns->mountMap, &nsMount->mapEntry);
        DEREF(nsMount->mount);
        free(nsMount);
    }

    if (ns->parent != NULL)
    {
        RWLOCK_WRITE_SCOPE(&ns->parent->lock);
        while (!list_is_empty(&ns->children))
        {
            namespace_t* child = CONTAINER_OF(list_pop_first(&ns->children), namespace_t, entry);
            list_push_back(&ns->parent->children, &child->entry);
            child->parent = ns->parent;
        }
        list_remove(&ns->parent->children, &ns->entry);
    }
}

uint64_t namespace_traverse_mount(namespace_t* ns, const path_t* mountpoint, path_t* out)
{
    if (mountpoint == NULL || mountpoint->mount == NULL || mountpoint->dentry == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_READ_SCOPE(&ns->lock);

    map_key_t key = mount_key(mountpoint->mount->id, mountpoint->dentry->id);
    map_entry_t* entry = map_get(&ns->mountMap, &key);
    if (entry == NULL)
    {
        path_copy(out, mountpoint);
        return 0;
    }

    namespace_mount_t* nsMount = CONTAINER_OF(entry, namespace_mount_t, mapEntry);
    path_set(out, nsMount->mount, nsMount->mount->root);
    return 0;
}

mount_t* namespace_mount(namespace_t* ns, path_t* mountpoint, const char* deviceName, const char* fsName, mount_flags_t flags, void* private)
{
    if (ns == NULL || deviceName == NULL || fsName == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    filesystem_t* fs = vfs_get_fs(fsName);
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
    DEREF_DEFER(root);

    if (atomic_load(&root->flags) & DENTRY_NEGATIVE)
    {
        errno = EIO; // This should never happen.
        return NULL;
    }

    if (mountpoint == NULL)
    {
        RWLOCK_WRITE_SCOPE(&ns->lock);
        if (ns->root != NULL)
        {
            errno = EBUSY;
            return NULL;
        }

        ns->root = mount_new(root->superblock, root, NULL, NULL);
        if (ns->root == NULL)
        {
            errno = ENOMEM;
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

    if (atomic_load(&mountpoint->dentry->flags) & DENTRY_NEGATIVE)
    {
        errno = ENOENT;
        return NULL;
    }

    mount_t* mount = mount_new(root->superblock, root, mountpoint->dentry, mountpoint->mount);
    if (mount == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    map_key_t key = mount_key(mountpoint->mount->id, mountpoint->dentry->id);
    if (namespace_add(ns, mount, flags, &key) == ERR)
    {
        DEREF(mount);
        errno = ENOMEM;
        return NULL;
    }

    // superblock_expose(superblock); // TODO: Expose the sysfsDir for the superblock

    LOG_DEBUG("mounted %s with %s\n", deviceName, fsName);
    return mount;
}

mount_t* namespace_bind(namespace_t* ns, dentry_t* source, path_t* mountpoint, mount_flags_t flags)
{
    if (ns == NULL || source == NULL || mountpoint == NULL || mountpoint->dentry == NULL || mountpoint->mount == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (atomic_load(&source->flags) & DENTRY_NEGATIVE)
    {
        errno = ENOENT;
        return NULL;
    }

    mount_t* mount = mount_new(source->superblock, source, mountpoint->dentry, mountpoint->mount);
    if (mount == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    map_key_t key = mount_key(mountpoint->mount->id, mountpoint->dentry->id);
    if (namespace_add(ns, mount, flags, &key) == ERR)
    {
        DEREF(mount);
        errno = ENOMEM;
        return NULL;
    }

    return mount;
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

    path_t cwd = cwd_get(&process->cwd);
    PATH_DEFER(&cwd);

    path_t mountPath = PATH_EMPTY;
    if (path_walk(&mountPath, &pathname, &cwd, WALK_NONE, &process->ns) == ERR)
    {
        return ERR;
    }

    file_t* sourceFile = file_table_get(&process->fileTable, source);
    if (sourceFile == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(sourceFile);

    mount_t* bind = namespace_bind(&process->ns, sourceFile->path.dentry, &mountPath, flags);
    if (bind == NULL)
    {
        return ERR;
    }
    DEREF(bind);
    return 0;
}

uint64_t namespace_get_root_path(namespace_t* ns, path_t* out)
{
    if (out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    rwlock_read_acquire(&ns->lock);
    if (ns->root == NULL || ns->root->superblock == NULL || ns->root->root == NULL)
    {
        rwlock_read_release(&ns->lock);
        errno = ENOENT;
        return ERR;
    }
    path_set(out, ns->root, ns->root->root);
    rwlock_read_release(&ns->lock);
    return 0;
}
