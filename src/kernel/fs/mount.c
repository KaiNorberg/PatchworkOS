#include <kernel/fs/mount.h>

#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <stdlib.h>
#include <sys/list.h>

static void mount_free(mount_t* mount)
{
    if (mount == NULL)
    {
        return;
    }

    if (mount->superblock != NULL)
    {
        superblock_dec_mount_count(mount->superblock);
        UNREF(mount->superblock);
    }

    if (mount->target != NULL)
    {
        atomic_fetch_sub_explicit(&mount->target->mountCount, 1, memory_order_relaxed);
        UNREF(mount->target);
    }

    if (mount->source != NULL)
    {
        UNREF(mount->source);
    }

    if (mount->parent != NULL)
    {
        UNREF(mount->parent);
    }

    free(mount);
}

mount_t* mount_new(superblock_t* superblock, dentry_t* source, dentry_t* target, mount_t* parent, mode_t mode)
{
    if (superblock == NULL || source == NULL || (target != NULL && parent == NULL))
    {
        errno = EINVAL;
        return NULL;
    }

    if (!DENTRY_IS_POSITIVE(source) || (target != NULL && !DENTRY_IS_POSITIVE(target)))
    {
        errno = ENOENT;
        return NULL;
    }

    mount_t* mount = malloc(sizeof(mount_t));
    if (mount == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    ref_init(&mount->ref, mount_free);
    mount->id = vfs_id_get();
    mount->source = REF(source);
    if (target != NULL)
    {
        mount->target = REF(target);
        atomic_fetch_add_explicit(&target->mountCount, 1, memory_order_relaxed);
    }
    else
    {
        mount->target = NULL;
    }
    superblock_inc_mount_count(superblock);
    mount->superblock = REF(superblock);
    mount->parent = parent != NULL ? REF(parent) : NULL;
    mount->mode = mode;

    return mount;
}
