#include <kernel/fs/inode.h>

#include <kernel/fs/vfs.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>

#include <stdlib.h>

static void inode_free(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

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
        UNREF(inode->superblock);

        if (inode->superblock->ops == NULL || inode->superblock->ops->freeInode == NULL)
        {
            free(inode);
        }

        return;
    }

    free(inode);
}

inode_t* inode_new(superblock_t* superblock, ino_t number, itype_t type, const inode_ops_t* ops,
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
    inode->number = number;
    inode->type = type;
    atomic_init(&inode->dentryCount, 0);
    inode->size = 0;
    inode->blocks = 0;
    inode->accessTime = 0;
    inode->modifyTime = inode->accessTime;
    inode->changeTime = inode->accessTime;
    inode->createTime = inode->accessTime;
    inode->private = NULL;
    inode->superblock = REF(superblock);
    inode->ops = ops;
    inode->fileOps = fileOps;
    mutex_init(&inode->mutex);

    return inode;
}

void inode_notify_access(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    MUTEX_SCOPE(&inode->mutex);

    inode->accessTime = clock_epoch();
}

void inode_notify_modify(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    MUTEX_SCOPE(&inode->mutex);
    inode->modifyTime = clock_epoch();
    inode->changeTime = inode->modifyTime;
}

void inode_notify_change(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    MUTEX_SCOPE(&inode->mutex);
    inode->changeTime = clock_epoch();
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

ino_t ino_gen(ino_t parentNumber, const char* name)
{
    uint64_t hash = 0xcbf29ce484222325;
    const uint64_t prime = 0x100000001b3;

    for (int i = 0; i < 8; i++)
    {
        hash ^= (parentNumber >> (i * 8)) & 0xFF;
        hash *= prime;
    }

    while (*name)
    {
        hash ^= (uint8_t)*name++;
        hash *= prime;
    }

    return hash;
}