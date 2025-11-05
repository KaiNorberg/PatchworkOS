#include <kernel/fs/inode.h>

#include <kernel/fs/vfs.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>

#include <stdlib.h>

static void inode_free(inode_t* inode)
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

        if (inode->superblock->ops == NULL || inode->superblock->ops->freeInode == NULL)
        {
            free(inode);
        }

        return;
    }

    free(inode);
}

inode_t* inode_new(superblock_t* superblock, inode_number_t number, inode_type_t type, const inode_ops_t* ops,
    const file_ops_t* fileOps)
{
    if (superblock == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    inode_t* inode;
    if (superblock->ops != NULL && superblock->ops->allocInode != NULL)
    {
        inode = superblock->ops->allocInode(superblock);
    }
    else
    {
        inode = malloc(sizeof(inode_t));
    }

    if (inode == NULL)
    {
        return NULL;
    }

    ref_init(&inode->ref, inode_free);
    map_entry_init(&inode->mapEntry);
    inode->number = number;
    inode->type = type;
    inode->flags = INODE_NONE;
    inode->linkCount = 1;
    inode->size = 0;
    inode->blocks = 0;
    inode->accessTime = sys_time_unix_epoch();
    inode->modifyTime = inode->accessTime;
    inode->changeTime = inode->accessTime;
    inode->createTime = inode->accessTime;
    inode->private = NULL;
    inode->superblock = REF(superblock);
    inode->ops = ops;
    inode->fileOps = fileOps;
    mutex_init(&inode->mutex);

    if (vfs_add_inode(inode) == ERR)
    {
        DEREF(inode);
        return NULL;
    }

    return inode;
}

void inode_notify_access(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    MUTEX_SCOPE(&inode->mutex);

    inode->accessTime = sys_time_unix_epoch();
    // TODO: Sync to disk.
}

void inode_notify_modify(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    MUTEX_SCOPE(&inode->mutex);
    inode->modifyTime = sys_time_unix_epoch();
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
    inode->changeTime = sys_time_unix_epoch();
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
