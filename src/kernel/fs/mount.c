#include "mount.h"

#include "mem/heap.h"
#include "vfs.h"

mount_t* mount_new(superblock_t* superblock, path_t* mountpoint)
{
    mount_t* mount = heap_alloc(sizeof(mount_t), HEAP_NONE);
    if (mount == NULL)
    {
        return NULL;
    }

    map_entry_init(&mount->mapEntry);
    mount->id = vfs_get_new_id();
    atomic_init(&mount->ref, 1);
    mount->superblock = superblock_ref(superblock);
    mount->mountpoint = mountpoint != NULL ? dentry_ref(mountpoint->dentry) : NULL;
    mount->parent = mountpoint != NULL ? mount_ref(mountpoint->mount) : NULL;

    return mount;
}

void mount_free(mount_t* mount)
{
    if (mount == NULL)
    {
        return;
    }

    if (mount->superblock != NULL)
    {
        superblock_deref(mount->superblock);
    }

    if (mount->mountpoint != NULL)
    {
        dentry_deref(mount->mountpoint);
    }

    if (mount->parent != NULL)
    {
        mount_deref(mount->parent);
    }

    heap_free(mount);
}

mount_t* mount_ref(mount_t* mount)
{
    if (mount != NULL)
    {
        atomic_fetch_add_explicit(&mount->ref, 1, memory_order_relaxed);
    }
    return mount;
}

void mount_deref(mount_t* mount)
{
    if (mount != NULL && atomic_fetch_sub_explicit(&mount->ref, 1, memory_order_relaxed) <= 1)
    {
        atomic_thread_fence(memory_order_acquire);
        mount_free(mount);
    }
}
