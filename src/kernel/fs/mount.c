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

    if (mount->volume != NULL)
    {
        UNREF(mount->volume);
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

    rcu_call(&mount->rcu, rcu_call_free, mount);
}

mount_t* mount_new(volume_t* volume, dentry_t* source, dentry_t* target, mount_t* parent, mode_t mode)
{
    if (volume == NULL || source == NULL || (target != NULL && parent == NULL))
    {
        return NULL;
    }

    mount_t* mount = malloc(sizeof(mount_t));
    if (mount == NULL)
    {
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
    mount->volume = REF(volume);
    mount->parent = parent != NULL ? REF(parent) : NULL;
    mount->mode = mode;

    return mount;
}
