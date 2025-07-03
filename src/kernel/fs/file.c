#include "file.h"

#include "mem/heap.h"
#include "sched/thread.h"
#include "vfs.h"

file_t* file_new(dentry_t* dentry, path_flags_t flags)
{
    file_t* file = heap_alloc(sizeof(file_t), HEAP_NONE);
    if (file == NULL)
    {
        return NULL;
    }

    atomic_init(&file->ref, 1);
    file->pos = 0;
    file->flags = flags;
    file->dentry = dentry_ref(dentry);
    file->ops = dentry->inode->fileOps;
    file->private = NULL;

    return file;
}

void file_free(file_t* file)
{
    if (file == NULL)
    {
        return;
    }

    if (file->dentry != NULL)
    {
        dentry_deref(file->dentry);
    }

    if (file->ops != NULL && file->ops->cleanup != NULL)
    {
        file->ops->cleanup(file);
    }

    heap_free(file);
}

file_t* file_ref(file_t* file)
{
    if (file != NULL)
    {
        atomic_fetch_add(&file->ref, 1);
    }
    return file;
}

void file_deref(file_t* file)
{
    if (file != NULL && atomic_fetch_sub(&file->ref, 1) <= 1)
    {
        file_free(file);
    }
}

uint64_t file_generic_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    LOCK_DEFER(&file->dentry->inode->lock);
    uint64_t newPos;
    switch (origin)
    {
    case SEEK_SET:
        newPos = offset;
        break;
    case SEEK_CUR:
        newPos = file->pos + offset;
        break;
    case SEEK_END:
        newPos = file->dentry->inode->size + offset;
        break;
    default:
        errno = EINVAL;
        return ERR;
    }

    file->pos = newPos;
    return newPos;
}