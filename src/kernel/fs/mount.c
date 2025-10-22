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
        superblock_dec_mount_count(mount->superblock);
        DEREF(mount->superblock);
    }

    if (mount->mountpoint != NULL)
    {
        DEREF(mount->mountpoint);
    }

    if (mount->parent != NULL)
    {
        DEREF(mount->parent);
    }

    heap_free(mount);
}

mount_t* mount_new(superblock_t* superblock, dentry_t* root, dentry_t* mountpoint, mount_t* parent)
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
    superblock_inc_mount_count(superblock);
    mount->root = root != NULL ? REF(root) : NULL;
    mount->mountpoint = mountpoint != NULL ? REF(mountpoint) : NULL;
    mount->parent = parent != NULL ? REF(parent) : NULL;

    return mount;
}
