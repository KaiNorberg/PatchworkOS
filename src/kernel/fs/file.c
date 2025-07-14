#include "file.h"

#include "fs/path.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "vfs.h"

file_t* file_new(inode_t* inode, const path_t* path, path_flags_t flags)
{
    file_t* file = heap_alloc(sizeof(file_t), HEAP_NONE);
    if (file == NULL)
    {
        return NULL;
    }

    ref_init(&file->ref, file_free);
    file->pos = 0;
    file->flags = flags;
    file->inode = REF(inode);
    file->path = PATH_EMPTY;
    path_copy(&file->path, path);
    file->ops = inode->fileOps;
    file->private = NULL;

    return file;
}

void file_free(file_t* file)
{
    if (file == NULL)
    {
        return;
    }

    if (file->ops != NULL && file->ops->close != NULL)
    {
        file->ops->close(file);
    }

    DEREF(file->inode);
    file->inode = NULL;
    path_put(&file->path);

    heap_free(file);
}

uint64_t file_generic_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    LOCK_SCOPE(&file->inode->lock);
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
        newPos = file->inode->size + offset;
        break;
    default:
        errno = EINVAL;
        return ERR;
    }

    file->pos = newPos;
    return newPos;
}
