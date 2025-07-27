#include "inode.h"

#include "sched/timer.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "vfs.h"

inode_t* inode_new(superblock_t* superblock, inode_number_t number, inode_type_t type, const inode_ops_t* ops,
    const file_ops_t* fileOps)
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

    ref_init(&inode->ref, inode_free);
    map_entry_init(&inode->mapEntry);
    inode->number = number;
    inode->type = type;
    inode->flags = INODE_NONE;
    inode->linkCount = 1;
    inode->size = 0;
    inode->blocks = 0;
    inode->accessTime = timer_unix_epoch();
    inode->modifyTime = inode->accessTime;
    inode->changeTime = inode->accessTime;
    inode->createTime = inode->accessTime;
    inode->private = NULL;
    inode->superblock = REF(superblock);
    inode->ops = ops;
    inode->fileOps = fileOps;
    mutex_init(&inode->mutex);

    vfs_add_inode(inode);

    return inode;
}

void inode_free(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    vfs_remove_inode(inode);

    if (inode->ops != NULL && inode->ops->cleanup != NULL)
    {
        inode->ops->cleanup(inode);
    }

    if (inode->superblock != NULL)
    {
        if (inode->superblock->ops != NULL && inode->superblock->ops->freeInode != NULL)
        {
            inode->superblock->ops->freeInode(inode->superblock, inode);
        }
        DEREF(inode->superblock);
    }

    // If freeInode was not called cleanup manually.
    if (inode->superblock == NULL || inode->superblock->ops == NULL || inode->superblock->ops->freeInode == NULL)
    {
        heap_free(inode);
    }
}

void inode_notify_access(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    MUTEX_SCOPE(&inode->mutex);

    inode->accessTime = timer_unix_epoch();
    // TODO: Sync to disk.
}

void inode_notify_modify(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    MUTEX_SCOPE(&inode->mutex);
    inode->modifyTime = timer_unix_epoch();
    inode->changeTime = inode->modifyTime;
    // TODO: Sync to disk.
}

void inode_notify_change(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    MUTEX_SCOPE(&inode->mutex);
    inode->changeTime = timer_unix_epoch();
    // TODO: Sync to disk.
}

void inode_truncate(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    if (inode->ops != NULL && inode->ops->truncate != NULL)
    {
        MUTEX_SCOPE(&inode->mutex);
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        inode->ops->truncate(inode);
    }
}
