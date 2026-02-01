#include <kernel/fs/volume.h>

#include <kernel/fs/filesystem.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/mem/pmm.h>

#include <stdlib.h>

static void volume_free(volume_t* volume)
{
    if (volume == NULL)
    {
        return;
    }

    assert(atomic_load(&volume->mountCount) == 0);

    rwlock_write_acquire(&volume->fs->lock);
    list_remove(&volume->entry);
    rwlock_write_release(&volume->fs->lock);

    if (volume->ops != NULL && volume->ops->cleanup != NULL)
    {
        volume->ops->cleanup(volume);
    }

    volume->root = NULL;

    free(volume);
}

volume_t* volume_new(filesystem_t* fs, const volume_ops_t* ops, const dentry_ops_t* dentryOps)
{
    if (fs == NULL)
    {
        return NULL;
    }

    volume_t* volume = malloc(sizeof(volume_t));
    if (volume == NULL)
    {
        return NULL;
    }

    ref_init(&volume->ref, volume_free);
    list_entry_init(&volume->entry);
    volume->id = vfs_id_get();
    volume->data = NULL;
    volume->root = NULL;
    volume->ops = ops;
    volume->dentryOps = dentryOps;
    volume->fs = fs;

    rwlock_write_acquire(&fs->lock);
    list_push_back(&fs->volumes, &volume->entry);
    rwlock_write_release(&fs->lock);

    return volume;
}