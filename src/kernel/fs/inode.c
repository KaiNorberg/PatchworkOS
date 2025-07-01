#include "inode.h"

#include "drivers/systime/systime.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "vfs.h"

inode_t* inode_new(superblock_t* superblock, inode_type_t type, inode_ops_t* ops, file_ops_t* fileOps)
{
    if (superblock == NULL)
    {
        return NULL;
    }

    inode_t* inode;
    if (superblock->ops != NULL && superblock->ops->allocInode != NULL)
    {
        inode = superblock->ops->allocInode(superblock);
    }
    else
    {
        inode = heap_alloc(sizeof(inode_t), HEAP_NONE);
    }

    map_entry_init(&inode->mapEntry);
    inode->id = 0;
    atomic_init(&inode->ref, 1);
    inode->type = type;
    inode->flags = INODE_NONE;
    atomic_init(&inode->linkCount, 1);
    inode->size = 0;
    inode->blocks = 0;
    inode->blockSize = superblock->blockSize;
    inode->accessTime = systime_unix_epoch();
    inode->modifyTime = inode->accessTime;
    inode->changeTime = inode->accessTime;
    inode->private = NULL;
    inode->superblock = superblock_ref(superblock);
    inode->ops = ops;
    inode->fileOps = fileOps;
    lock_init(&inode->lock);

    return inode;
}

void inode_free(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    if (inode->superblock != NULL)
    {
        if (inode->superblock->ops != NULL && inode->superblock->ops->freeInode != NULL)
        {
            inode->superblock->ops->freeInode(inode->superblock, inode);
        }
        superblock_deref(inode->superblock);
    }

    // If freeInode was not called cleanup manually.
    if (inode->superblock == NULL || inode->superblock->ops == NULL || inode->superblock->ops->freeInode == NULL)
    {
        heap_free(inode);
    }
}

inode_t* inode_ref(inode_t* inode)
{
    if (inode != NULL)
    {
        atomic_fetch_add(&inode->ref, 1);
    }
    return inode;
}

void inode_deref(inode_t* inode)
{
    if (inode != NULL && atomic_fetch_sub(&inode->ref, 1) <= 1)
    {
        vfs_remove_inode(inode);
        inode_free(inode);
    }
}

uint64_t inode_sync(inode_t* inode)
{
    if (inode == NULL)
    {
        return ERROR(EINVAL);
    }

    if (inode->superblock->ops != NULL && inode->superblock->ops->writeInode != NULL)
    {
        return inode->superblock->ops->writeInode(inode->superblock, inode);
    }

    return 0;
}

void inode_access_time_update(inode_t* inode)
{
    LOCK_DEFER(&inode->lock);

    // TODO: Unsure if this is correct, investigate further.
    time_t now = systime_unix_epoch();
    if (inode->accessTime < inode->modifyTime ||
        inode->accessTime < inode->changeTime ||
        (now - inode->accessTime) > (24 * 60 * 60)) 
    {
        inode->accessTime = now;
    }
}

void inode_modify_time_update(inode_t* inode)
{
    LOCK_DEFER(&inode->lock);

    inode->modifyTime = systime_unix_epoch();
    inode->changeTime = inode->modifyTime;
    inode->accessTime = inode->modifyTime;
}

void inode_change_time_update(inode_t* inode)
{
    LOCK_DEFER(&inode->lock);

    inode->changeTime = systime_unix_epoch();
}