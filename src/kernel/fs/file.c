#include <kernel/fs/file.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
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

    DEREF(file->inode);
    file->inode = NULL;
    path_put(&file->path);

    free(file);
}

file_t* file_new(inode_t* inode, const path_t* path, mode_t mode)
{
    if (inode == NULL || path == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (((mode & MODE_ALL_PERMS) & ~path->mount->mode) != 0)
    {
        errno = EACCES;
        return NULL;
    }

    file_t* file = malloc(sizeof(file_t));
    if (file == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    if ((mode & MODE_ALL_PERMS) == MODE_NONE)
    {
        mode |= path->mount->mode & MODE_ALL_PERMS;
    }

    ref_init(&file->ref, file_free);
    file->pos = 0;
    file->mode = mode;
    file->inode = REF(inode);
    file->path = PATH_EMPTY;
    path_copy(&file->path, path);
    file->ops = inode->fileOps;
    file->private = NULL;

    return file;
}

uint64_t file_generic_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    MUTEX_SCOPE(&file->inode->mutex);

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
