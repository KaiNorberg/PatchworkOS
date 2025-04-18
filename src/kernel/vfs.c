#include "vfs.h"

#include "lock.h"
#include "path.h"
#include "sched.h"
#include "systime.h"
#include "vfs_ctx.h"
#include "waitsys.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static list_t volumes;
static lock_t volumesLock;

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
    LOCK_DEFER(&volumesLock);

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

file_t* file_new(volume_t* volume)
{
    file_t* file = malloc(sizeof(file_t));
    if (file == NULL)
    {
        return NULL;
    }

    file->volume = volume;
    file->pos = 0;
    file->private = NULL;
    file->resource = NULL;
    file->ops = NULL;
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
        if (file->volume->ops->cleanup != NULL)
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
    printf("vfs: init");

    list_init(&volumes);
    lock_init(&volumesLock);
}

uint64_t vfs_attach_simple(const char* label, const volume_ops_t* ops)
{
    if (strlen(label) >= MAX_NAME)
    {
        return ERROR(EINVAL);
    }
    LOCK_DEFER(&volumesLock);

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

    list_push(&volumes, &volume->entry);
    return 0;
}

uint64_t vfs_mount(const char* label, fs_t* fs)
{
    return fs->mount(label);
}

uint64_t vfs_unmount(const char* label)
{
    LOCK_DEFER(&volumesLock);

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
    free(volume);
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

uint64_t vfs_listdir(const char* path, dir_entry_t* entries, uint64_t amount)
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

    if (volume->ops->listdir == NULL)
    {
        volume_deref(volume);
        return ERROR(ENOOP);
    }

    uint64_t result = volume->ops->listdir(volume, &parsedPath, entries, amount);
    volume_deref(volume);
    return result;
}

uint64_t vfs_read(file_t* file, void* buffer, uint64_t count)
{
    if (file->resource != NULL && atomic_load(&file->resource->hidden))
    {
        return ERROR(ENORES);
    }
    if (file->ops->read == NULL)
    {
        return ERROR(ENOOP);
    }
    return file->ops->read(file, buffer, count);
}

uint64_t vfs_write(file_t* file, const void* buffer, uint64_t count)
{
    if (file->resource != NULL && atomic_load(&file->resource->hidden))
    {
        return ERROR(ENORES);
    }
    if (file->ops->write == NULL)
    {
        return ERROR(ENOOP);
    }
    return file->ops->write(file, buffer, count);
}

uint64_t vfs_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    if (file->resource != NULL && atomic_load(&file->resource->hidden))
    {
        return ERROR(ENORES);
    }
    if (file->ops->seek == NULL)
    {
        return ERROR(ENOOP);
    }
    return file->ops->seek(file, offset, origin);
}

uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    if (file->resource != NULL && atomic_load(&file->resource->hidden))
    {
        return ERROR(ENORES);
    }
    if (file->ops->ioctl == NULL)
    {
        return ERROR(ENOOP);
    }
    return file->ops->ioctl(file, request, argp, size);
}

uint64_t vfs_flush(file_t* file, const void* buffer, uint64_t size, const rect_t* rect)
{
    if (file->resource != NULL && atomic_load(&file->resource->hidden))
    {
        return ERROR(ENORES);
    }
    if (file->ops->flush == NULL)
    {
        return ERROR(ENOOP);
    }
    return file->ops->flush(file, buffer, size, rect);
}

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, nsec_t timeout)
{
    if (amount > CONFIG_MAX_BLOCKERS_PER_THREAD)
    {
        return ERROR(EBLOCKLIMIT);
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        if (files[i].file == NULL)
        {
            return ERROR(EINVAL);
        }
        if (files[i].file->resource != NULL && atomic_load(&files[i].file->resource->hidden))
        {
            return ERROR(ENORES);
        }
        if (files[i].file->ops->poll == NULL)
        {
            return ERROR(ENOOP);
        }
        files[i].occurred = 0;
    }

    wait_queue_t* waitQueues[CONFIG_MAX_BLOCKERS_PER_THREAD];
    for (uint64_t i = 0; i < amount; i++)
    {
        waitQueues[i] = files[i].file->ops->poll(files[i].file, &files[i]);
        if (waitQueues[i] == NULL)
        {
            return ERR;
        }
    }

    uint64_t events = 0;
    uint64_t currentTime = systime_uptime();
    uint64_t deadline = timeout == NEVER ? NEVER : currentTime + timeout;
    while (true)
    {
        events = 0;
        for (uint64_t i = 0; i < amount; i++)
        {
            if (files[i].file == NULL || files[i].file->ops == NULL)
            {
                return ERROR(EINVAL);
            }

            if (files[i].file->ops->poll(files[i].file, &files[i]) == NULL)
            {
                return ERR;
            }

            if ((files[i].occurred & files[i].requested) != 0)
            {
                events++;
            }
        }
        if (events != 0 || currentTime >= deadline)
        {
            break;
        }

        nsec_t remainingTime = deadline == NEVER ? NEVER : deadline - currentTime;
        waitsys_block_many(waitQueues, amount, remainingTime);

        currentTime = systime_uptime();
    }

    return events;
}

void dir_entry_push(dir_entry_t* entries, uint64_t amount, uint64_t* index, uint64_t* total, dir_entry_t* entry)
{
    if (*index < amount)
    {
        entries[(*index)++] = *entry;
    }

    (*total)++;
}
