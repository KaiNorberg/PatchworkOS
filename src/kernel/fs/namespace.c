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

#include <errno.h>

static map_key_t mount_cache_key(mount_id_t parentId, dentry_id_t mountpointId)
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

void namespace_init(namespace_t* ns, namespace_t* parent, process_t* owner)
{
    if (ns == NULL)
    {
        return;
    }

    map_init(&ns->mountPoints);
    rwlock_init(&ns->lock);
    ns->parent = parent;
    ns->owner = owner;
    ns->rootMount = (parent != NULL && parent->rootMount != NULL) ? REF(parent->rootMount) : NULL;
}

void namespace_deinit(namespace_t* ns)
{
    if (ns == NULL)
    {
        return;
    }

    DEREF(ns->rootMount);

    rwlock_write_acquire(&ns->lock);
    for (uint64_t i = 0; i < ns->mountPoints.capacity; i++)
    {
        map_entry_t* entry = ns->mountPoints.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }
        mount_t* mount = CONTAINER_OF(entry, mount_t, mapEntry);
        DEREF(mount);
    }
    rwlock_write_release(&ns->lock);

    map_deinit(&ns->mountPoints);
}

uint64_t namespace_traverse_mount(namespace_t* ns, const path_t* mountpoint, path_t* outRoot)
{
    if (mountpoint == NULL || mountpoint->mount == NULL || mountpoint->dentry == NULL || outRoot == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    map_key_t key = mount_cache_key(mountpoint->mount->id, mountpoint->dentry->id);
    namespace_t* currentNs = ns;
    while (currentNs != NULL)
    {
        RWLOCK_READ_SCOPE(&currentNs->lock);

        mount_t* mount = CONTAINER_OF_SAFE(map_get(&currentNs->mountPoints, &key), mount_t, mapEntry);
        if (mount != NULL)
        {
            if (mount->superblock == NULL || mount->root == NULL)
            {
                errno = ESTALE;
                return ERR;
            }

            path_set(outRoot, mount, mount->root);
            return 0;
        }

        currentNs = currentNs->parent;
    }

    path_copy(outRoot, mountpoint);
    return 0;
}

mount_t* namespace_mount(namespace_t* ns, path_t* mountpoint, const char* deviceName, const char* fsName, void* private)
{
    if (deviceName == NULL || fsName == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (ns == NULL)
    {
        process_t* kernelProcess = process_get_kernel();
        assert(kernelProcess != NULL);
        ns = &kernelProcess->ns;
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
        if (ns->rootMount != NULL)
        {
            errno = EBUSY;
            return NULL;
        }

        ns->rootMount = mount_new(root->superblock, root, NULL, NULL);
        if (ns->rootMount == NULL)
        {
            return NULL;
        }

        LOG_INFO("mounted %s as root with %s\n", deviceName, fsName);
        return REF(ns->rootMount);
    }

    if (ns->rootMount == NULL)
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
        return NULL;
    }

    map_key_t key = mount_cache_key(mountpoint->mount->id, mountpoint->dentry->id);
    rwlock_write_acquire(&ns->lock);
    if (map_insert(&ns->mountPoints, &key, &mount->mapEntry) == ERR)
    {
        DEREF(mount);
        rwlock_write_release(&ns->lock);
        return NULL;
    }
    rwlock_write_release(&ns->lock);

    // superblock_expose(superblock); // TODO: Expose the sysfsDir for the superblock

    LOG_DEBUG("mounted %s with %s\n", deviceName, fsName);
    return REF(mount);
}

mount_t* namespace_bind(namespace_t* ns, dentry_t* source, path_t* mountpoint)
{
    if (source == NULL || mountpoint == NULL || mountpoint->dentry == NULL || mountpoint->mount == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (atomic_load(&source->flags) & DENTRY_NEGATIVE)
    {
        errno = ENOENT;
        return NULL;
    }

    if (ns == NULL)
    {
        process_t* kernelProcess = process_get_kernel();
        assert(kernelProcess != NULL);
        ns = &kernelProcess->ns;
    }

    mount_t* mount = mount_new(source->superblock, source, mountpoint->dentry, mountpoint->mount);
    if (mount == NULL)
    {
        return NULL;
    }

    map_key_t key = mount_cache_key(mountpoint->mount->id, mountpoint->dentry->id);
    rwlock_write_acquire(&ns->lock);
    if (map_insert(&ns->mountPoints, &key, &mount->mapEntry) == ERR)
    {
        DEREF(mount);
        rwlock_write_release(&ns->lock);
        return NULL;
    }
    rwlock_write_release(&ns->lock);

    return REF(mount);
}

SYSCALL_DEFINE(SYS_BIND, uint64_t, fd_t source, const char* mountpointString)
{
    if (mountpointString == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t pathname;
    if (thread_copy_from_user_pathname(thread, &pathname, mountpointString) == ERR)
    {
        return ERR;
    }

    path_t cwd = cwd_get(&process->cwd);
    PATH_DEFER(&cwd);

    path_t mountpoint;
    if (path_walk(&mountpoint, &pathname, &cwd, WALK_NONE, &process->ns) == ERR)
    {
        return ERR;
    }

    file_t* sourceFile = file_table_get(&process->fileTable, source);
    if (sourceFile == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(sourceFile);

    mount_t* bind = namespace_bind(&process->ns, sourceFile->path.dentry, &mountpoint);
    if (bind == NULL)
    {
        return ERR;
    }
    DEREF(bind);
    return 0;
}

uint64_t namespace_get_root_path(namespace_t* ns, path_t* outPath)
{
    if (outPath == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (ns == NULL)
    {
        process_t* kernelProcess = process_get_kernel();
        assert(kernelProcess != NULL);
        ns = &kernelProcess->ns;
    }

    rwlock_read_acquire(&ns->lock);
    if (ns->rootMount == NULL || ns->rootMount->superblock == NULL || ns->rootMount->root == NULL)
    {
        rwlock_read_release(&ns->lock);
        errno = ENOENT;
        return ERR;
    }
    path_set(outPath, ns->rootMount, ns->rootMount->root);
    rwlock_read_release(&ns->lock);
    return 0;
}
