#include <kernel/fs/vnode.h>

#include <kernel/fs/vfs.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/mem/cache.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>

#include <stdlib.h>

static void vnode_free(vnode_t* vnode)
{
    assert(vnode != NULL);
    rcu_call(&vnode->rcu, rcu_call_cache_free, vnode);
}

static void vnode_reclaim(vnode_t* vnode)
{
    assert(vnode != NULL);

    if (vnode->cls->handlers[IRP_MJ_RECLAIM] != NULL)
    {
        // Revive reference
        ref_init(&vnode->ref, vnode_free);

        irp_prep_reclaim(vnode->reclaim);
        vnode_call(vnode, vnode->reclaim);

        UNREF(vnode);
        return;
    }

    if (vnode->reclaim != NULL)
    {
        irp_complete(vnode->reclaim, OK);
        vnode->reclaim = NULL;
    }

    vnode_free(vnode);
}

static cache_t cache = CACHE_CREATE(cache, "vnode", sizeof(vnode_t), CACHE_LINE, NULL, NULL);

vnode_t* vnode_new(file_volume_t volume, const vnode_class_t* cls, file_number_t number)
{
    if (cls == NULL)
    {
        return NULL;
    }

    vnode_t* vnode = cache_alloc(cls->cache != NULL ? cls->cache : &cache);
    if (vnode == NULL)
    {
        return NULL;
    }

    vnode->data = NULL;
    mutex_init(&vnode->mutex);

    ref_init(&vnode->ref, vnode_reclaim);
    vnode->volume = volume;
    vnode->number = number;
    vnode->cls = cls;
    vnode->reclaim = irp_new(process_get_kernel(), NULL);
    if (vnode->reclaim == NULL)
    {
        vnode_free(vnode);
        return NULL;
    }
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

status_t vnode_generic_attr(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    vnode_t* vnode = frame->vnode;

    switch (frame->attr.attr)
    {
    case FILE_GET_VOLUME:
        irp->result = vnode->volume;
        return OK;
    case FILE_GET_NUMBER:
        irp->result = vnode->number;
        return OK;
    case FILE_GET_TYPE:
        irp->result = vnode->cls->type;
        return OK;
    default:
        return ERR(FS, INVAL);
    }
}

status_t vnode_generic_query(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    vnode_t* vnode = frame->vnode;

    file_info_t info = {0};

    if (frame->file != NULL && DENTRY_IS_POSITIVE(frame->file->path.dentry))
    {
        strncpy(info.name, frame->file->path.dentry->name.data, MIN(sizeof(info.name), frame->file->path.dentry->name.length));
        info.mask |= FILE_MASK_NAME;
    }

    info.type = vnode->cls->type;
    info.mask |= FILE_MASK_TYPE;

    info.volume = vnode->volume;
    info.mask |= FILE_MASK_VOLUME;

    info.number = vnode->number;
    info.mask |= FILE_MASK_NUMBER;

    return sglist_copy_in(frame->query.buffer, sizeof(file_info_t), 0, &irp->result, &info, sizeof(file_info_t));
}

status_t vnode_generic_dir_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;

    if (file == NULL)
    {
        return ERR(FS, INVAL);
    }

    dentry_t* dentry = file->path.dentry;
    vnode_t* vnode = dentry->vnode;

    diremit_t emit;
    diremit_begin(&emit, irp);

    mutex_acquire(&vnode->mutex);

    if (!diremit(&emit, "."))
    {
        goto done;
    }
    if (!diremit(&emit, ".."))
    {
        goto done;
    }

    dentry_t* child;
    LIST_FOR_EACH(child, &dentry->children, siblingEntry)
    {
        if (DENTRY_IS_POSITIVE(child))
        {
            if (!diremit(&emit, child->name.data))
            {
                goto done;
            }
        }
    }

    status_t status;
done:
    status = diremit_end(&emit);
    mutex_release(&vnode->mutex);
    return status;
}

status_t vnode_generic_lookup(irp_t* irp)
{
    UNUSED(irp);

    return ERR(FS, NOENT);
}

file_number_t vnode_hash(file_number_t parent, const char* name)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint64_t prime = 0x100000001b3ULL;

    for (size_t i = 0; i < sizeof(file_number_t); i++)
    {
        hash ^= ((uint8_t*)&parent)[i];
        hash *= prime;
    }

    for (size_t i = 0; name[i] != '\0'; i++)
    {
        hash ^= (uint8_t)name[i];
        hash *= prime;
    }

    return hash;
}
