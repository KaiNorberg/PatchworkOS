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

    if (vnode->cls->cleanup != NULL)
    {
        vnode->cls->cleanup(vnode);
    }
    vnode->data = NULL;

    if (vnode->volume != NULL)
    {
        UNREF(vnode->volume);
        vnode->volume = NULL;
    }

    rcu_call(&vnode->rcu, rcu_call_cache_free, vnode);
}

static void vnode_ctor(void* ptr)
{
    vnode_t* vnode = (vnode_t*)ptr;

    vnode->ref = (ref_t){0};
    atomic_init(&vnode->dentryCount, 0);
    vnode->data = NULL;
    vnode->size = 0;
    vnode->volume = NULL;
    vnode->cls = NULL;
    vnode->rcu = (rcu_entry_t){0};
    mutex_init(&vnode->mutex);
}

static cache_t cache = CACHE_CREATE(cache, "vnode", sizeof(vnode_t), CACHE_LINE, vnode_ctor, NULL);

vnode_t* vnode_new(volume_t* volume, const vnode_class_t* cls)
{
    if (volume == NULL || cls == NULL)
    {
        return NULL;
    }

    vnode_t* vnode = cache_alloc(&cache);
    if (vnode == NULL)
    {
        return NULL;
    }

    ref_init(&vnode->ref, vnode_free);
    vnode->volume = REF(volume);
    vnode->cls = cls;
    return vnode;
}

void vnode_call(vnode_t* vnode, irp_t* irp)
{
    assert(irp->frame > 0);
    irp->frame--;

    irp_frame_t* frame = irp_current(irp);
    if (UNLIKELY(frame->major >= IRP_MJ_MAX))
    {
        irp_complete(irp, ERR(IO, MJ_OVERFLOW));
        return;
    }

    if (vnode == NULL || vnode->cls == NULL)
    {
        irp_complete(irp, ERR(IO, MJ_NOSYS));
        return;
    }

    irp_func_t func = vnode->cls->handlers[frame->major];
    if (func == NULL)
    {
        irp_complete(irp, ERR(IO, MJ_NOSYS));
        return;
    }

    atomic_store_explicit(&irp->cancel, NULL, memory_order_relaxed);
    frame->vnode = REF(vnode);

    func(irp);
}

void vnode_truncate(vnode_t* vnode)
{
    if (vnode == NULL)
    {
        return;
    }

    if (vnode->cls->truncate != NULL)
    {
        MUTEX_SCOPE(&vnode->mutex);
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        vnode->cls->truncate(vnode);
    }
}