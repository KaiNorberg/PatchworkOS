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

status_t vnode_call(vnode_t* vnode, irp_t* irp)
{
    assert(vnode != NULL);
    assert(irp != NULL);

    irp_handler_t handler = NULL;

    irp_frame_t* frame = irp_next(irp);
    if (LIKELY(frame->major < IRP_MJ_MAX) && vnode->cls->handlers[frame->major] != NULL)
    {
        handler = vnode->cls->handlers[frame->major];
    }

    frame->vnode = REF(vnode);
    frame->file = NULL;

    return irp_call(irp, handler);
}
