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

    atomic_init(&file->ref, 1);
    file->pos = 0;
    file->flags = flags;
    file->inode = inode_ref(inode);
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

    if (file->ops != NULL && file->ops->cleanup != NULL)
    {
        file->ops->cleanup(file);
    }

    inode_deref(file->inode);
    file->inode = NULL;
    path_put(&file->path);

    heap_free(file);
}

file_t* file_ref(file_t* file)
{
    if (file != NULL)
    {
        atomic_fetch_add_explicit(&file->ref, 1, memory_order_relaxed);
    }
    return file;
}

void file_deref(file_t* file)
{
    if (file == NULL)
    {
        return;
    }

    uint64_t ref = atomic_fetch_sub_explicit(&file->ref, 1, memory_order_relaxed);
    if (ref <= 1)
    {
        atomic_thread_fence(memory_order_acquire);
        assert(ref == 1); // Check for double free.
        file_free(file);
    }
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
