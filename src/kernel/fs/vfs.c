#include "vfs.h"

#include "sync/lock.h"
#include "utils/log.h"
#include "path.h"
#include "sync/rwlock.h"
#include "sched/sched.h"
#include "sys/list.h"
#include "sysfs.h"
#include "drivers/systime/systime.h"
#include "vfs_ctx.h"
#include "sched/wait.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static list_t volumes;
static rwlock_t volumesLock;

static void volume_on_free(sysdir_t* dir)
{
    volume_t* volume = dir->private;
    free(volume);
}

static uint64_t volume_expose(volume_t* volume)
{
    volume->dir = sysdir_new("/vol", volume->label, volume_on_free, volume);
    if (volume->dir == NULL)
    {
        return ERR;
    }

    return 0;
}

static volume_t* volume_ref(volume_t* volume)
{
    atomic_fetch_add(&volume->ref, 1);
    return volume;
}

static void volume_deref(volume_t* volume)
{
    atomic_fetch_sub(&volume->ref, 1);
}

static volume_t* volume_get(const char* label)
{
    RWLOCK_READ_DEFER(&volumesLock);

    volume_t* volume;
    LIST_FOR_EACH(volume, &volumes, entry)
    {
        if (strcmp(volume->label, label) == 0)
        {
            return volume_ref(volume);
        }
    }

    return NULL;
}

file_t* file_new(volume_t* volume, const path_t* path, path_flags_t supportedFlags)
{
    path_flags_t flags = path != NULL ? path->flags : 0;
    if ((flags & ~supportedFlags) != 0)
    {
        return ERRPTR(EINVAL);
    }

    file_t* file = malloc(sizeof(file_t));
    if (file == NULL)
    {
        return NULL;
    }

    file->volume = volume;
    file->pos = 0;
    file->private = NULL;
    file->sysobj = NULL;
    file->ops = NULL;
    file->flags = flags;
    atomic_init(&file->ref, 1);

    return file;
}

file_t* file_ref(file_t* file)
{
    atomic_fetch_add(&file->ref, 1);
    return file;
}

void file_deref(file_t* file)
{
    if (atomic_fetch_sub(&file->ref, 1) <= 1)
    {
        if (file->volume->ops != NULL && file->volume->ops->cleanup != NULL)
        {
            file->volume->ops->cleanup(file->volume, file);
        }
        if (file->volume != NULL)
        {
            volume_deref(file->volume);
        }
        free(file);
    }
}

static uint64_t vfs_parse_path(path_t* out, const char* path)
{
    vfs_ctx_t* ctx = &sched_process()->vfsCtx;
    LOCK_DEFER(&ctx->lock);
    return path_init(out, path, &ctx->cwd);
}

void vfs_init(void)
{
    printf("vfs: init\n");

    list_init(&volumes);
    rwlock_init(&volumesLock);
}

uint64_t vfs_attach_simple(const char* label, const volume_ops_t* ops)
{
    if (strlen(label) >= MAX_NAME)
    {
        return ERROR(EINVAL);
    }
    RWLOCK_WRITE_DEFER(&volumesLock);

    volume_t* volume;
    LIST_FOR_EACH(volume, &volumes, entry)
    {
        if (strcmp(volume->label, label) == 0)
        {
            return ERROR(EEXIST);
        }
    }

    volume = malloc(sizeof(volume_t));
    list_entry_init(&volume->entry);
    strcpy(volume->label, label);
    volume->ops = ops;
    atomic_init(&volume->ref, 1);
    if (volume_expose(volume) == ERR)
    {
        free(volume);
        return ERR;
    }
    list_push(&volumes, &volume->entry);
    return 0;
}

uint64_t vfs_mount(const char* label, fs_t* fs)
{
    return fs->mount(label);
}

uint64_t vfs_unmount(const char* label)
{
    RWLOCK_WRITE_DEFER(&volumesLock);

    volume_t* volume;
    bool found = false;
    LIST_FOR_EACH(volume, &volumes, entry)
    {
        if (strcmp(volume->label, label) == 0)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        return ERROR(EPATH);
    }

    if (atomic_load(&volume->ref) != 1)
    {
        return ERROR(EBUSY);
    }

    if (volume->ops->unmount == NULL)
    {
        return ERROR(ENOOP);
    }

    if (volume->ops->unmount(volume) == ERR)
    {
        return ERR;
    }

    list_remove(&volume->entry);
    sysdir_free(volume->dir);
    return 0;
}

uint64_t vfs_chdir(const char* path)
{
    stat_t info;
    if (vfs_stat(path, &info) == ERR)
    {
        return ERR;
    }

    if (info.type != STAT_DIR)
    {
        return ERROR(ENOTDIR);
    }

    path_t parsedPath;
    if (vfs_parse_path(&parsedPath, path) == ERR)
    {
        return ERR;
    }

    vfs_ctx_t* ctx = &sched_process()->vfsCtx;
    LOCK_DEFER(&ctx->lock);
    ctx->cwd = parsedPath;
    return 0;
}

file_t* vfs_open(const char* path)
{
    path_t parsedPath;
    if (vfs_parse_path(&parsedPath, path) == ERR)
    {
        return NULL;
    }

    volume_t* volume = volume_get(parsedPath.volume);
    if (volume == NULL)
    {
        return ERRPTR(EPATH);
    }

    if (volume->ops->open == NULL)
    {
        volume_deref(volume);
        return ERRPTR(ENOOP);
    }

    file_t* file = volume->ops->open(volume, &parsedPath);
    if (file == NULL)
    {
        volume_deref(volume);
        return NULL;
    }

    return file;
}

uint64_t vfs_open2(const char* path, file_t* files[2])
{
    path_t parsedPath;
    if (vfs_parse_path(&parsedPath, path) == ERR)
    {
        return ERR;
    }

    volume_t* volume = volume_get(parsedPath.volume);
    if (volume == NULL)
    {
        return ERROR(EPATH);
    }

    if (volume->ops->open2 == NULL)
    {
        volume_deref(volume);
        return ERROR(ENOOP);
    }

    uint64_t result = volume->ops->open2(volume, &parsedPath, files);
    if (result == ERR)
    {
        volume_deref(volume);
        return ERR;
    }

    return result;
}

uint64_t vfs_stat(const char* path, stat_t* buffer)
{
    path_t parsedPath;
    if (vfs_parse_path(&parsedPath, path) == ERR)
    {
        return ERR;
    }

    volume_t* volume = volume_get(parsedPath.volume);
    if (volume == NULL)
    {
        return ERROR(EPATH);
    }

    if (volume->ops->stat == NULL)
    {
        volume_deref(volume);
        return ERROR(ENOOP);
    }

    uint64_t result = volume->ops->stat(volume, &parsedPath, buffer);
    volume_deref(volume);
    return result;
}

uint64_t vfs_rename(const char* oldpath, const char* newpath)
{
    path_t parsedOldPath;
    if (vfs_parse_path(&parsedOldPath, oldpath) == ERR)
    {
        return ERR;
    }
    path_t parsedNewPath;
    if (vfs_parse_path(&parsedNewPath, newpath) == ERR)
    {
        return ERR;
    }

    volume_t* oldVolume = volume_get(parsedOldPath.volume);
    if (oldVolume == NULL)
    {
        return ERROR(EPATH);
    }

    volume_t* newVolume = volume_get(parsedNewPath.volume);
    if (newVolume == NULL)
    {
        volume_deref(oldVolume);
        return ERROR(EPATH);
    }

    if (oldVolume->ops->rename == NULL)
    {
        volume_deref(oldVolume);
        volume_deref(newVolume);
        return ERROR(ENOOP);
    }

    if (oldVolume != newVolume)
    {
        volume_deref(oldVolume);
        volume_deref(newVolume);
        return ERROR(EXDEV);
    }

    uint64_t result = oldVolume->ops->rename(oldVolume, &parsedOldPath, &parsedNewPath);
    volume_deref(oldVolume);
    volume_deref(newVolume);
    return result;
}

uint64_t vfs_remove(const char* path)
{
    path_t parsedPath;
    if (vfs_parse_path(&parsedPath, path) == ERR)
    {
        return ERR;
    }

    volume_t* volume = volume_get(parsedPath.volume);
    if (volume == NULL)
    {
        return ERROR(EPATH);
    }

    if (volume->ops->remove == NULL)
    {
        volume_deref(volume);
        return ERROR(ENOOP);
    }

    uint64_t result = volume->ops->remove(volume, &parsedPath);
    volume_deref(volume);
    return result;
}

uint64_t vfs_readdir(file_t* file, stat_t* infos, uint64_t amount)
{
    if (file->sysobj != NULL && atomic_load(&file->sysobj->hidden))
    {
        return ERROR(ENOOBJ);
    }
    if (file->ops->readdir == NULL)
    {
        return ERROR(ENOOP);
    }
    return file->ops->readdir(file, infos, amount);
}

uint64_t vfs_read(file_t* file, void* buffer, uint64_t count)
{
    if (file->sysobj != NULL && atomic_load(&file->sysobj->hidden))
    {
        return ERROR(ENOOBJ);
    }
    if (file->ops->read == NULL)
    {
        return ERROR(ENOOP);
    }
    return file->ops->read(file, buffer, count);
}

uint64_t vfs_write(file_t* file, const void* buffer, uint64_t count)
{
    if (file->sysobj != NULL && atomic_load(&file->sysobj->hidden))
    {
        return ERROR(ENOOBJ);
    }
    if (file->ops->write == NULL)
    {
        return ERROR(ENOOP);
    }
    return file->ops->write(file, buffer, count);
}

uint64_t vfs_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    if (file->sysobj != NULL && atomic_load(&file->sysobj->hidden))
    {
        return ERROR(ENOOBJ);
    }
    if (file->ops->seek == NULL)
    {
        return ERROR(ENOOP);
    }
    return file->ops->seek(file, offset, origin);
}

uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    if (file->sysobj != NULL && atomic_load(&file->sysobj->hidden))
    {
        return ERROR(ENOOBJ);
    }
    if (file->ops->ioctl == NULL)
    {
        return ERROR(ENOOP);
    }
    return file->ops->ioctl(file, request, argp, size);
}

void* vfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    if (file->sysobj != NULL && atomic_load(&file->sysobj->hidden))
    {
        return ERRPTR(ENOOBJ);
    }
    if (file->ops->mmap == NULL)
    {
        return ERRPTR(ENOOP);
    }
    return file->ops->mmap(file, address, length, prot);
}

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, clock_t timeout)
{
    for (uint64_t i = 0; i < amount; i++)
    {
        if (files[i].file == NULL)
        {
            return ERROR(EINVAL);
        }
        if (files[i].file->sysobj != NULL && atomic_load(&files[i].file->sysobj->hidden))
        {
            return ERROR(ENOOBJ);
        }
        if (files[i].file->ops->poll == NULL)
        {
            return ERROR(ENOOP);
        }
        files[i].occurred = 0;
    }

    wait_queue_t** waitQueues = malloc(sizeof(wait_queue_t) * amount);
    if (waitQueues == NULL)
    {
        return ERR;
    }

    uint64_t events = 0;
    uint64_t currentTime = systime_uptime();
    uint64_t deadline = timeout == CLOCKS_NEVER ? CLOCKS_NEVER : currentTime + timeout;
    while (true)
    {
        events = 0;
        bool shouldBlock = true;
        for (uint64_t i = 0; i < amount; i++)
        {
            if (files[i].file == NULL || files[i].file->ops == NULL)
            {
                free(waitQueues);
                return ERROR(EINVAL);
            }

            waitQueues[i] = files[i].file->ops->poll(files[i].file, &files[i]);
            if (waitQueues[i] == NULL)
            {
                free(waitQueues);
                return ERR;
            }

            if ((files[i].occurred & files[i].requested) != 0)
            {
                events++;
            }

            if (files[i].file->flags & PATH_NONBLOCK)
            {
                shouldBlock = false;
            }
        }
        if (events != 0 || currentTime >= deadline)
        {
            break;
        }

        if (!shouldBlock)
        {
            free(waitQueues);
            return ERROR(EWOULDBLOCK);
        }

        clock_t remainingTime = deadline == CLOCKS_NEVER ? CLOCKS_NEVER : deadline - currentTime;
        wait_block_many(waitQueues, amount, remainingTime);

        currentTime = systime_uptime();
    }

    free(waitQueues);
    return events;
}

void readdir_push(stat_t* infos, uint64_t amount, uint64_t* index, uint64_t* total, stat_t* info)
{
    if (*index < amount)
    {
        infos[(*index)++] = *info;
    }

    (*total)++;
}
