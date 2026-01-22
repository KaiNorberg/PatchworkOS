#include <kernel/fs/inode.h>

#include <kernel/fs/vfs.h>
#include <kernel/mem/cache.h>
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
    inode->data = NULL;

    if (inode->superblock != NULL)
    {
        UNREF(inode->superblock);
        inode->superblock = NULL;
    }

    rcu_call(&inode->rcu, rcu_call_cache_free, inode);
}

static void inode_ctor(void* ptr)
{
    inode_t* inode = (inode_t*)ptr;

    inode->ref = (ref_t){0};
    inode->number = 0;
    inode->type = 0;
    atomic_init(&inode->dentryCount, 0);
    inode->size = 0;
    inode->blocks = 0;
    inode->accessTime = 0;
    inode->modifyTime = 0;
    inode->changeTime = 0;
    inode->createTime = 0;
    inode->data = NULL;
    inode->superblock = NULL;
    inode->ops = NULL;
    inode->fileOps = NULL;
    inode->rcu = (rcu_entry_t){0};
    mutex_init(&inode->mutex);
}

static cache_t cache = CACHE_CREATE(cache, "inode", sizeof(inode_t), CACHE_LINE, inode_ctor, NULL);

inode_t* inode_new(superblock_t* superblock, ino_t number, itype_t type, const inode_ops_t* ops,
    const file_ops_t* fileOps)
{
    if (superblock == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    inode_t* inode = cache_alloc(&cache);
    if (inode == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    ref_init(&inode->ref, inode_free);
    inode->number = number;
    inode->type = type;
    inode->superblock = REF(superblock);
    inode->ops = ops;
    inode->fileOps = fileOps;
    inode->verbs = superblock->verbs;
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

    hash ^= parentNumber;
    hash *= prime;

    while (*name != '\0')
    {
        hash ^= (uint8_t)*name++;
        hash *= prime;
    }

    return hash;
}