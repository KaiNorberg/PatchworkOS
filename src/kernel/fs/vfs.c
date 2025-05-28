#include "vfs.h"

#include "drivers/systime/systime.h"
#include "path.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "sync/lock.h"
#include "sync/rwlock.h"
#include "sys/list.h"
#include "sysfs.h"
#include "utils/log.h"
#include "vfs_ctx.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static list_t volumes;
static rwlock_t volumesLock;

static uint64_t volume_expose(volume_t* volume)
{
    assert(sysdir_init(&volume->sysdir, "/vol", volume->label, volume) != ERR);
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
    file->syshdr = NULL;
    file->ops = NULL;
    file->flags = flags;
    atomic_init(&file->ref, 1);
    file->path = *path;

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

static void volume_on_free(sysdir_t* dir)
{
    volume_t* volume = dir->private;
    free(volume);
}

uint64_t vfs_unmount(const char* label)
{
    RWLOCK_WRITE_DEFER(&volumesLock);

    volume_t* volume;
    bool isFound = false;
    LIST_FOR_EACH(volume, &volumes, entry)
    {
        if (strcmp(volume->label, label) == 0)
        {
            isFound = true;
            break;
        }
    }

    if (!isFound)
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
    sysdir_deinit(&volume->sysdir, volume_on_free);
    return 0;
}

uint64_t vfs_chdir(const path_t* path)
{
    if (path == NULL || path->isInvalid)
    {
        return ERROR(EPATH);
    }

    stat_t info;
    if (vfs_stat(path, &info) == ERR)
    {
        return ERR;
    }

    if (info.type != STAT_DIR)
    {
        return ERROR(ENOTDIR);
    }

    vfs_ctx_t* ctx = &sched_process()->vfsCtx;
    LOCK_DEFER(&ctx->lock);
    ctx->cwd = *path;
    return 0;
}

file_t* vfs_open(const path_t* path)
{
    if (path == NULL || path->isInvalid)
    {
        return ERRPTR(EPATH);
    }

    volume_t* volume = volume_get(path->volume);
    if (volume == NULL)
    {
        return ERRPTR(EPATH);
    }

    if (volume->ops->open == NULL)
    {
        volume_deref(volume);
        return ERRPTR(ENOOP);
    }

    file_t* file = volume->ops->open(volume, path);
    if (file == NULL)
    {
        volume_deref(volume);
        return NULL;
    }

    return file;
}

uint64_t vfs_open2(const path_t* path, file_t* files[2])
{
    if (path == NULL || path->isInvalid)
    {
        return ERROR(EPATH);
    }

    volume_t* volume = volume_get(path->volume);
    if (volume == NULL)
    {
        return ERROR(EPATH);
    }

    if (volume->ops->open2 == NULL)
    {
        volume_deref(volume);
        return ERROR(ENOOP);
    }

    uint64_t result = volume->ops->open2(volume, path, files);
    if (result == ERR)
    {
        volume_deref(volume);
        return ERR;
    }

    return result;
}

uint64_t vfs_stat(const path_t* path, stat_t* buffer)
{
    if (path == NULL || path->isInvalid)
    {
        return ERROR(EPATH);
    }

    volume_t* volume = volume_get(path->volume);
    if (volume == NULL)
    {
        return ERROR(EPATH);
    }

    if (volume->ops->stat == NULL)
    {
        volume_deref(volume);
        return ERROR(ENOOP);
    }

    uint64_t result = volume->ops->stat(volume, path, buffer);
    volume_deref(volume);
    return result;
}

uint64_t vfs_rename(const path_t* oldpath, const path_t* newpath)
{
    if (oldpath == NULL || oldpath->isInvalid || newpath == NULL || newpath->isInvalid)
    {
        return ERROR(EPATH);
    }

    if (strcmp(oldpath->volume, newpath->volume) != 0)
    {
        return ERROR(EXDEV);
    }

    volume_t* volume = volume_get(oldpath->volume);
    if (volume == NULL)
    {
        return ERROR(EPATH);
    }

    if (volume->ops->rename == NULL)
    {
        volume_deref(volume);
        return ERROR(ENOOP);
    }

    uint64_t result = volume->ops->rename(volume, oldpath, newpath);
    volume_deref(volume);
    return result;
}

uint64_t vfs_remove(const path_t* path)
{
    if (path == NULL || path->isInvalid)
    {
        return ERROR(EPATH);
    }

    volume_t* volume = volume_get(path->volume);
    if (volume == NULL)
    {
        return ERROR(EPATH);
    }

    if (volume->ops->remove == NULL)
    {
        volume_deref(volume);
        return ERROR(ENOOP);
    }

    uint64_t result = volume->ops->remove(volume, path);
    volume_deref(volume);
    return result;
}

uint64_t vfs_readdir(file_t* file, stat_t* infos, uint64_t amount)
{
    if (file->ops->readdir == NULL)
    {
        return ERROR(ENOOP);
    }

    if (file->syshdr == NULL)
    {
        return file->ops->readdir(file, infos, amount);
    }
    else
    {
        if (sysfs_start_op(file) == ERR)
        {
            return ERR;
        }
        uint64_t result = file->ops->readdir(file, infos, amount);
        sysfs_end_op(file);
        return result;
    }
}

uint64_t vfs_read(file_t* file, void* buffer, uint64_t count)
{
    if (file->ops->read == NULL)
    {
        return ERROR(ENOOP);
    }

    if (file->syshdr == NULL)
    {
        return file->ops->read(file, buffer, count);
    }
    else
    {
        if (sysfs_start_op(file) == ERR)
        {
            return ERR;
        }
        uint64_t result = file->ops->read(file, buffer, count);
        sysfs_end_op(file);
        return result;
    }
}

uint64_t vfs_write(file_t* file, const void* buffer, uint64_t count)
{
    if (file->ops->write == NULL)
    {
        return ERROR(ENOOP);
    }

    if (file->syshdr == NULL)
    {
        return file->ops->write(file, buffer, count);
    }
    else
    {
        if (sysfs_start_op(file) == ERR)
        {
            return ERR;
        }
        uint64_t result = file->ops->write(file, buffer, count);
        sysfs_end_op(file);
        return result;
    }
}

uint64_t vfs_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    if (file->ops->seek == NULL)
    {
        return ERROR(ENOOP);
    }

    if (file->syshdr == NULL)
    {
        return file->ops->seek(file, offset, origin);
    }
    else
    {
        if (sysfs_start_op(file) == ERR)
        {
            return ERR;
        }
        uint64_t result = file->ops->seek(file, offset, origin);
        sysfs_end_op(file);
        return result;
    }
}

uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    if (file->ops->ioctl == NULL)
    {
        return ERROR(ENOOP);
    }

    if (file->syshdr == NULL)
    {
        return file->ops->ioctl(file, request, argp, size);
    }
    else
    {
        if (sysfs_start_op(file) == ERR)
        {
            return ERR;
        }
        uint64_t result = file->ops->ioctl(file, request, argp, size);
        sysfs_end_op(file);
        return result;
    }
}

void* vfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    if (file->ops->mmap == NULL)
    {
        return ERRPTR(ENOOP);
    }

    if (file->syshdr == NULL)
    {
        return file->ops->mmap(file, address, length, prot);
    }
    else
    {
        if (sysfs_start_op(file) == ERR)
        {
            return NULL;
        }
        void* result = file->ops->mmap(file, address, length, prot);
        sysfs_end_op(file);
        return result;
    }
}

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, clock_t timeout)
{
    for (uint64_t i = 0; i < amount; i++)
    {
        if (files[i].file == NULL)
        {
            return ERROR(EINVAL);
        }
        if (files[i].file->ops == NULL || files[i].file->ops->poll == NULL)
        {
            return ERROR(ENOOP);
        }
        files[i].revents = 0;
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

            if (files[i].file->syshdr == NULL)
            {
                waitQueues[i] = files[i].file->ops->poll(files[i].file, &files[i]);
            }
            else
            {
                if (sysfs_start_op(files[i].file) == ERR)
                {
                    free(waitQueues);
                    return ERR;
                }
                waitQueues[i] = files[i].file->ops->poll(files[i].file, &files[i]);
                sysfs_end_op(files[i].file);
            }

            if (waitQueues[i] == NULL)
            {
                free(waitQueues);
                return ERR;
            }

            if ((files[i].revents & files[i].events) != 0)
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
