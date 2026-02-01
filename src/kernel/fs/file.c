#include <kernel/fs/file.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/volume.h>
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