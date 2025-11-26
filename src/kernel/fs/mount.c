#include <kernel/fs/mount.h>

#include <kernel/fs/vfs.h>

#include <stdlib.h>

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
        dentry_dec_mount_count(mount->mountpoint);
        DEREF(mount->mountpoint);
    }

    if (mount->root != NULL)
    {
        DEREF(mount->root);
    }

    if (mount->parent != NULL)
    {
        DEREF(mount->parent);
    }

    free(mount);
}

mount_t* mount_new(superblock_t* superblock, dentry_t* root, dentry_t* mountpoint, mount_t* parent)
{
    if (superblock == NULL)
    {
        return NULL;
    }

    mount_t* mount = malloc(sizeof(mount_t));
    if (mount == NULL)
    {
        return NULL;
    }

    ref_init(&mount->ref, mount_free);
    mount->id = vfs_get_new_id();
    mount->superblock = REF(superblock);
    superblock_inc_mount_count(superblock);
    if (mountpoint != NULL)
    {
        mount->mountpoint = REF(mountpoint);
        dentry_inc_mount_count(mountpoint);
    }
    else
    {
        mount->mountpoint = NULL;
    }
    mount->root = root != NULL ? REF(root) : NULL;
    mount->parent = parent != NULL ? REF(parent) : NULL;

    return mount;
}
