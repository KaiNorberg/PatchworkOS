#include <kernel/fs/file.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/irp.h>
#include <kernel/mem/cache.h>
#include <kernel/mem/mdl.h>
#include <kernel/proc/process.h>
#include <kernel/sync/mutex.h>
#include <kernel/utils/ref.h>

#include <sys/status.h>
#include <errno.h>
#include <stdlib.h>

static void file_free(file_t* file)
{
    if (file == NULL)
    {
        return;
    }

    if (file->ops != NULL && file->ops->close != NULL)
    {
        file->ops->close(file);
    }

    UNREF(file->vnode);
    file->vnode = NULL;
    path_put(&file->path);

    cache_free(file);
}

static cache_t cache = CACHE_CREATE(cache, "file", sizeof(file_t), CACHE_LINE, NULL, NULL);

file_t* file_new(const path_t* path, mode_t mode)
{
    file_t* file = cache_alloc(&cache);
    if (file == NULL)
    {
        return NULL;
    }

    ref_init(&file->ref, file_free);
    file->pos = 0;
    file->mode = mode;
    file->vnode = REF(path->dentry->vnode);
    file->path = PATH_CREATE(path->mount, path->dentry);
    file->ops = path->dentry->vnode->fileOps;
    file->data = NULL;
    return file;
}

status_t file_generic_seek(file_t* file, ssize_t offset, seek_origin_t origin, size_t* newPos)
{
    MUTEX_SCOPE(&file->vnode->mutex);

    size_t pos;
    switch (origin)
    {
    case SEEK_SET:
        pos = offset;
        break;
    case SEEK_CUR:
        pos = file->pos + offset;
        break;
    case SEEK_END:
        pos = file->vnode->size + offset;
        break;
    default:
        return ERR(IO, INVAL);
    }

    file->pos = pos;
    if (newPos != NULL)
    {
        *newPos = pos;
    }
    return OK;
}