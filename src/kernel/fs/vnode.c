#include <kernel/fs/vnode.h>

#include <kernel/fs/vfs.h>
#include <kernel/mem/cache.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>

#include <stdlib.h>

static void vnode_free(vnode_t* vnode)
{
    assert(vnode != NULL);
    cache_free(vnode);
}

static void vnode_reclaim(vnode_t* vnode)
{
    assert(vnode != NULL);

    // Revive reference
    ref_init(&vnode->ref, vnode_free); 

    irp_prep_reclaim(vnode->reclaim);
    vnode_call(vnode, vnode->reclaim);

    UNREF(vnode);
}

static void vnode_ctor(void* ptr)
{
    vnode_t* vnode = (vnode_t*)ptr;

    vnode->data = NULL;
    mutex_init(&vnode->mutex);
}

static cache_t cache = CACHE_CREATE(cache, "vnode", sizeof(vnode_t), CACHE_LINE, vnode_ctor, NULL);

vnode_t* vnode_new(fsvol_t vol, const vnode_class_t* cls, vnum_t num)
{
    if (cls == NULL)
    {
        return NULL;
    }

    vnode_t* vnode = cache_alloc(&cache);
    if (vnode == NULL)
    {
        return NULL;
    }

    ref_init(&vnode->ref, vnode_reclaim);
    vnode->vol = vol;
    vnode->num = num;
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
    case VATTR_GET_TYPE:
        irp->result = vnode->cls->type;
        return OK;
    case VATTR_GET_VOL:
        irp->result = vnode->volume->id;
        return OK;
    case VATTR_GET_NUM:
        if (frame->file != NULL)
        {
            irp->result = frame->file->path.dentry->id;
            return OK;
        }
        return ERR(FS, EXPECT_FILE);
    default:
        return ERR(FS, INVAL);
    }
}

status_t vnode_generic_query(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    vnode_t* vnode = frame->vnode;
    file_t* file = frame->file;

    vinfo_t info = {0};

    info.type = vnode->cls->type;
    info.valid |= VMASK_TYPE;

    info.vol = vnode->vol;
    info.valid |= VMASK_VOL;

    info.num = vnode->num;
    info.valid |= VMASK_NUM;

    return mdl_copy_in(frame->query.buffer, sizeof(vinfo_t), 0, &irp->result, &info, sizeof(vinfo_t));
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
            if (!diremit(&emit, child->name))
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

vnum_t vnum_hash(vnum_t parent, const char* name, size_t length)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint64_t prime = 0x100000001b3ULL;

    for (size_t i = 0; i < sizeof(vnum_t); i++)
    {
        hash ^= ((uint8_t*)&parent)[i];
        hash *= prime;
    }

    for (size_t i = 0; i < length; i++)
    {
        hash ^= (uint8_t)name[i];
        hash *= prime;
    }

    return hash;
}