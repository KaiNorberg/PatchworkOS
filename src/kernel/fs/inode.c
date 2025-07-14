#include "inode.h"

#include "drivers/systime/systime.h"
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
    inode->accessTime = systime_unix_epoch();
    inode->modifyTime = inode->accessTime;
    inode->changeTime = inode->accessTime;
    inode->private = NULL;
    inode->superblock = REF(superblock);
    inode->ops = ops;
    inode->fileOps = fileOps;
    lock_init(&inode->lock);

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
