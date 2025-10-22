#include "namespace.h"

#include "cpu/syscalls.h"
#include "dentry.h"
#include "log/log.h"
#include "mount.h"
#include "path.h"
#include "superblock.h"
#include "vfs.h"

#include <errno.h>

static map_key_t mount_cache_key(mount_id_t parentId, dentry_id_t mountpointId)
{
    uint64_t buffer[2] = {(uint64_t)parentId, (uint64_t)mountpointId};

    return map_key_buffer(buffer, sizeof(buffer));
}

uint64_t namespace_init(namespace_t* ns, namespace_t* parent, process_t* owner)
{
    if (ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (map_init(&ns->mountPoints) == ERR)
    {
        return ERR;
    }

    rwlock_init(&ns->lock);
    ns->parent = parent;
    ns->owner = owner;
    ns->rootMount = (parent != NULL && parent->rootMount != NULL) ? REF(parent->rootMount) : NULL;
    return 0;
}

void namespace_deinit(namespace_t* ns)
{
    if (ns == NULL)
    {
        return;
    }

    DEREF(ns->rootMount);

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
            if (atomic_load(&mount->ref.count) == 0) // Is currently being removed
            {
                errno = ESTALE;
                return ERR;
            }

            if (mount->superblock == NULL || mount->superblock->root == NULL)
            {
                errno = ESTALE;
                return ERR;
            }

            path_set(outRoot, mount, mount->superblock->root);
            return 0;
        }

        currentNs = currentNs->parent;
    }

    path_copy(outRoot, mountpoint);
    return 0;
}

uint64_t namespace_mount(namespace_t* ns, path_t* mountpoint, const char* deviceName, const char* fsName,
    mount_t** outRoot, void* private)
{
    if (deviceName == NULL || fsName == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (ns == NULL)
    {
        process_t* kernelProcess = process_get_kernel();
        assert(kernelProcess != NULL);
        ns = &kernelProcess->namespace;
    }

    filesystem_t* fs = vfs_get_fs(fsName);
    if (fs == NULL)
    {
        errno = ENODEV;
        return ERR;
    }

    mutex_acquire(&mountpoint->dentry->mutex);
    if (mountpoint->dentry->flags & DENTRY_NEGATIVE)
    {
        mutex_release(&mountpoint->dentry->mutex);
        errno = ENOENT;
        return ERR;
    }
    mutex_release(&mountpoint->dentry->mutex);

    dentry_t* root = fs->mount(fs, deviceName, private);
    if (root == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(root);
    MUTEX_SCOPE(&root->mutex);

    if (root->flags & DENTRY_NEGATIVE)
    {
        errno = EIO; // This should never happen.
        return ERR;
    }

    if (mountpoint == NULL)
    {
        if (ns->rootMount != NULL) // No need for the lock as this will only be set during initialization.
        {
            errno = EBUSY;
            return ERR;
        }

        ns->rootMount = mount_new(root->superblock, NULL);
        if (ns->rootMount == NULL)
        {
            return ERR;
        }

        if (outRoot != NULL)
        {
            *outRoot = REF(ns->rootMount);
        }

        LOG_INFO("mounted %s as root with %s\n", deviceName, fsName);
        return 0;
    }

    if (ns->rootMount == NULL)
    {
        errno = ENOENT;
        return ERR;
    }

    MUTEX_SCOPE(&mountpoint->dentry->mutex);

    mount_t* mount = mount_new(root->superblock, mountpoint);
    if (mount == NULL)
    {
        return ERR;
    }

    map_key_t key = mount_cache_key(mountpoint->mount->id, mountpoint->dentry->id);
    rwlock_write_acquire(&ns->lock);
    if (map_insert(&ns->mountPoints, &key, &mount->mapEntry) == ERR)
    {
        DEREF(mount);
        rwlock_write_release(&ns->lock);
        return ERR;
    }
    rwlock_write_release(&ns->lock);

    mountpoint->dentry->mountCount++;

    // superblock_expose(superblock); // TODO: Expose the sysfsDir for the superblock

    if (outRoot != NULL)
    {
        *outRoot = REF(mount);
    }

    LOG_INFO("mounted %s with %s\n", deviceName, fsName);
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
        ns = &kernelProcess->namespace;
    }

    rwlock_read_acquire(&ns->lock);
    if (ns->rootMount == NULL || ns->rootMount->superblock == NULL || ns->rootMount->superblock->root == NULL)
    {
        rwlock_read_release(&ns->lock);
        errno = ENOENT;
        return ERR;
    }
    path_set(outPath, ns->rootMount, ns->rootMount->superblock->root);
    rwlock_read_release(&ns->lock);
    return 0;
}
