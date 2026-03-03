#include <kernel/fs/file.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/binding.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/irp.h>
#include <kernel/mem/cache.h>
#include <kernel/mem/mdl.h>
#include <kernel/proc/process.h>
#include <kernel/sync/mutex.h>
#include <kernel/utils/ref.h>

#include <stdlib.h>
#include <sys/status.h>

static void file_free(file_t* file)
{
    assert(file != NULL);

    path_put(&file->path);

    cache_free(file);
}

static void file_close(file_t* file)
{
    assert(file != NULL);

    // Revive reference
    ref_init(&file->ref, file_free); 

    irp_prep_close(file->close);
    file_call(file, file->close);

    UNREF(file);
}

static cache_t cache = CACHE_CREATE(cache, "file", sizeof(file_t), CACHE_LINE, NULL, NULL);

file_t* file_new(dentry_t* dentry, binding_t* mount, mode_t mode)
{
    file_t* file = cache_alloc(&cache);
    if (file == NULL)
    {
        return NULL;
    }

    ref_init(&file->ref, file_close);
    file->pos = 0;
    file->mode = mode;
    file->path = PATH_CREATE(mount, dentry);
    file->data = NULL;
    file->close = irp_new(process_get_kernel(), NULL);
    if (file->close == NULL)
    {
        file_free(file);
        return NULL;
    }

    return file;
}

status_t file_call(file_t* file, irp_t* irp)
{
    assert(file != NULL);
    assert(irp != NULL);

    irp_handler_t handler = NULL;

    if (!DENTRY_IS_POSITIVE(file->path.dentry))
    {
        return ERR(FS, NOENT);
    }

    vnode_t* vnode = file->path.dentry->vnode;

    irp_frame_t* frame = irp_next(irp);
    if (LIKELY(frame->major < IRP_MJ_MAX) && vnode->cls->handlers[frame->major] != NULL)
    {
        handler = vnode->cls->handlers[frame->major];
    }

    frame->vnode = REF(vnode);
    frame->file = REF(file);

    if (frame->flags & IRP_FLAG_USE_FILE_POS)
    {
        switch (frame->major)
        {
        case IRP_MJ_READ:
            frame->read.offset = &file->pos;
            break;
        case IRP_MJ_WRITE:
            frame->write.offset = &file->pos;
            break;
        default:
            break;
        }
    }

    return irp_call(irp, handler);
}