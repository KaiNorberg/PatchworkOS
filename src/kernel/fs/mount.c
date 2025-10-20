#include "mount.h"

#include "mem/heap.h"
#include "vfs.h"

static void mount_free(mount_t* mount)
{
    if (mount == NULL)
    {
        return;
    }

    if (mount->superblock != NULL)
    {
        DEREF(mount->superblock);
    }

    if (mount->dentry != NULL)
    {
        DEREF(mount->dentry);
    }

    if (mount->parent != NULL)
    {
        DEREF(mount->parent);
    }

    heap_free(mount);
}

mount_t* mount_new(superblock_t* superblock, path_t* mountpoint)
{
    mount_t* mount = heap_alloc(sizeof(mount_t), HEAP_NONE);
    if (mount == NULL)
    {
        return NULL;
    }

    ref_init(&mount->ref, mount_free);
    map_entry_init(&mount->mapEntry);
    mount->id = vfs_get_new_id();
    mount->superblock = REF(superblock);
    mount->dentry = mountpoint != NULL ? REF(mountpoint->dentry) : NULL;
    mount->parent = mountpoint != NULL ? REF(mountpoint->mount) : NULL;

    return mount;
}
