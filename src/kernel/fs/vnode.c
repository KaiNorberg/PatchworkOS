#include <kernel/fs/vnode.h>

#include <kernel/fs/vfs.h>
#include <kernel/mem/cache.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>

#include <stdlib.h>

static void vnode_free(vnode_t* vnode)
{
    if (vnode == NULL)
    {
        return;
    }

    if (vnode->ops != NULL && vnode->ops->cleanup != NULL)
    {
        vnode->ops->cleanup(vnode);
    }
    vnode->data = NULL;

    if (vnode->superblock != NULL)
    {
        UNREF(vnode->superblock);
        vnode->superblock = NULL;
    }

    rcu_call(&vnode->rcu, rcu_call_cache_free, vnode);
}

static void vnode_ctor(void* ptr)
{
    vnode_t* vnode = (vnode_t*)ptr;

    vnode->ref = (ref_t){0};
    vnode->type = 0;
    atomic_init(&vnode->dentryCount, 0);
    vnode->data = NULL;
    vnode->size = 0;
    vnode->superblock = NULL;
    vnode->ops = NULL;
    vnode->fileOps = NULL;
    vnode->rcu = (rcu_entry_t){0};
    mutex_init(&vnode->mutex);
}

static cache_t cache = CACHE_CREATE(cache, "vnode", sizeof(vnode_t), CACHE_LINE, vnode_ctor, NULL);

vnode_t* vnode_new(superblock_t* superblock, vtype_t type, const vnode_ops_t* ops, const file_ops_t* fileOps)
{
    if (superblock == NULL)
    {
        return NULL;
    }

    vnode_t* vnode = cache_alloc(&cache);
    if (vnode == NULL)
    {
        return NULL;
    }

    ref_init(&vnode->ref, vnode_free);
    vnode->type = type;
    vnode->superblock = REF(superblock);
    vnode->ops = ops;
    vnode->fileOps = fileOps;
    vnode->vtable = NULL;
    return vnode;
}

void vnode_truncate(vnode_t* vnode)
{
    if (vnode == NULL)
    {
        return;
    }

    if (vnode->ops != NULL && vnode->ops->truncate != NULL)
    {
        MUTEX_SCOPE(&vnode->mutex);
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        vnode->ops->truncate(vnode);
    }
}