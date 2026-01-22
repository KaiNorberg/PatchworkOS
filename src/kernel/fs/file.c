#include <kernel/fs/file.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/io/irp.h>
#include <kernel/mem/cache.h>
#include <kernel/mem/mdl.h>
#include <kernel/proc/process.h>
#include <kernel/sync/mutex.h>
#include <kernel/utils/ref.h>

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

    UNREF(file->inode);
    file->inode = NULL;
    path_put(&file->path);

    cache_free(file);
}

static cache_t cache = CACHE_CREATE(cache, "file", sizeof(file_t), CACHE_LINE, NULL, NULL);

file_t* file_new(const path_t* path, mode_t mode)
{
    if (path == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (mode_check(&mode, path->mount->mode) == ERR)
    {
        return NULL;
    }

    if (!DENTRY_IS_POSITIVE(path->dentry))
    {
        errno = ENOENT;
        return NULL;
    }

    file_t* file = cache_alloc(&cache);
    if (file == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    ref_init(&file->ref, file_free);
    file->pos = 0;
    file->mode = mode;
    file->inode = REF(path->dentry->inode);
    file->path = PATH_CREATE(path->mount, path->dentry);
    file->ops = path->dentry->inode->fileOps;
    file->verbs = file->inode->verbs;
    file->data = NULL;
    return file;
}

size_t file_generic_seek(file_t* file, ssize_t offset, seek_origin_t origin)
{
    MUTEX_SCOPE(&file->inode->mutex);

    size_t newPos;
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