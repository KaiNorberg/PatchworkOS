#include <kernel/fs/superblock.h>

#include <kernel/fs/filesystem.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/mem/pmm.h>

#include <stdlib.h>

static void superblock_free(superblock_t* superblock)
{
    if (superblock == NULL)
    {
        return;
    }

    assert(atomic_load(&superblock->mountCount) == 0);

    rwlock_write_acquire(&superblock->fs->lock);
    list_remove(&superblock->fs->superblocks, &superblock->entry);
    rwlock_write_release(&superblock->fs->lock);

    if (superblock->ops != NULL && superblock->ops->cleanup != NULL)
    {
        superblock->ops->cleanup(superblock);
    }

    superblock->root = NULL;

    free(superblock);
}

superblock_t* superblock_new(filesystem_t* fs, block_device_t* device, const superblock_ops_t* ops,
    const dentry_ops_t* dentryOps)
{
    if (fs == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    superblock_t* superblock = malloc(sizeof(superblock_t));
    if (superblock == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    ref_init(&superblock->ref, superblock_free);
    list_entry_init(&superblock->entry);
    superblock->id = vfs_id_get();
    superblock->blockSize = PAGE_SIZE;
    superblock->maxFileSize = UINT64_MAX;
    superblock->private = NULL;
    superblock->root = NULL;
    superblock->device = device;
    superblock->ops = ops;
    superblock->dentryOps = dentryOps;
    superblock->fs = fs;
    atomic_init(&superblock->mountCount, 0);
    return superblock;
}

void superblock_inc_mount_count(superblock_t* superblock)
{
    atomic_fetch_add(&superblock->mountCount, 1);
}

void superblock_dec_mount_count(superblock_t* superblock)
{
    if (atomic_fetch_sub(&superblock->mountCount, 1) == 1)
    {
        if (superblock->ops != NULL && superblock->ops->unmount != NULL)
        {
            superblock->ops->unmount(superblock);
        }
    }
}
