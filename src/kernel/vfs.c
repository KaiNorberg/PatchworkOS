#include "vfs.h"

#include "lock.h"
#include "sched.h"
#include "systime.h"
#include "vfs_context.h"
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
        if (label_compare(volume->label, label))
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
        if (file->ops->cleanup != NULL)
        {
            file->ops->cleanup(file);
        }
        if (file->volume != NULL)
        {
            volume_deref(file->volume);
        }
        free(file);
    }
}

static uint64_t vfs_make_canonical(const char* start, char* out, const char* path)
{
    const char* name = path;
    while (true)
    {
        if (name_compare(name, "."))
        {
            // Do nothing
        }
        else if (name_compare(name, ".."))
        {
            out--;
            while (*out != VFS_NAME_SEPARATOR)
            {
                if (out <= start)
                {
                    return ERR;
                }

                out--;
            }
            out[1] = '\0';
        }
        else
        {
            const char* ptr = name;
            while (!VFS_END_OF_NAME(*ptr))
            {
                if (!VFS_VALID_CHAR(*ptr) || (uint64_t)(out - start) >= MAX_PATH - 2)
                {
                    return ERR;
                }

                *++out = *ptr++;
            }

            out++;
            out[0] = VFS_NAME_SEPARATOR;
            out[1] = '\0';
        }

        name = name_next(name);
        if (name == NULL || name[0] == '\0')
        {
            if (*out == VFS_NAME_SEPARATOR)
            {
                *out = '\0';
            }
            return 0;
        }
    }
}

static uint64_t vfs_parse_path(char* out, const char* path)
{
    vfs_context_t* context = &sched_process()->vfsContext;
    LOCK_DEFER(&context->lock);

    if (path[0] == VFS_NAME_SEPARATOR) // Root path
    {
        uint64_t labelLength = strchr(context->cwd, VFS_LABEL_SEPARATOR) - context->cwd;
        memcpy(out, context->cwd, labelLength);

        out[labelLength] = ':';
        out[labelLength + 1] = '\0';

        return vfs_make_canonical(out + labelLength, out + labelLength, path);
    }

    bool absolute = false;
    uint64_t i = 0;
    for (; !VFS_END_OF_NAME(path[i]); i++)
    {
        if (path[i] == VFS_LABEL_SEPARATOR)
        {
            if (!VFS_END_OF_NAME(path[i + 1]))
            {
                return ERR;
            }

            absolute = true;
            break;
        }
        else if (!VFS_VALID_CHAR(path[i]))
        {
            return ERR;
        }
    }

    if (absolute) // Absolute path
    {
        uint64_t labelLength = i;
        memcpy(out, path, labelLength);

        out[labelLength] = ':';
        out[labelLength + 1] = '/';
        out[labelLength + 2] = '\0';

        return vfs_make_canonical(out + labelLength, out + labelLength, path + labelLength + 1);
    }
    else // Relative path
    {
        uint64_t labelLength = strchr(context->cwd, VFS_LABEL_SEPARATOR) - context->cwd;
        uint64_t cwdLength = strlen(context->cwd);

        memcpy(out, context->cwd, cwdLength + 1);

        out[cwdLength] = VFS_NAME_SEPARATOR;
        out[cwdLength + 1] = '\0';

        return vfs_make_canonical(out + labelLength, out + cwdLength, path);
    }
}

void vfs_init(void)
{
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
        if (name_compare(volume->label, label))
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
        if (name_compare(volume->label, label))
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
        return ERROR(EACCES);
    }

    if (volume->ops->unmount(volume) == ERR)
    {
        return ERR;
    }

    list_remove(&volume->entry);
    free(volume);
    return 0;
}

uint64_t vfs_realpath(char* out, const char* path)
{
    if (vfs_parse_path(out, path) == ERR)
    {
        return ERROR(EPATH);
    }
    else
    {
        return 0;
    }
}

uint64_t vfs_chdir(const char* path)
{
    char parsedPath[MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
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

    vfs_context_t* context = &sched_process()->vfsContext;
    LOCK_DEFER(&context->lock);

    strcpy(context->cwd, parsedPath);
    return 0;
}

file_t* vfs_open(const char* path)
{
    char parsedPath[MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return ERRPTR(EPATH);
    }

    volume_t* volume = volume_get(parsedPath);
    if (volume == NULL)
    {
        return ERRPTR(EPATH);
    }

    if (volume->ops->open == NULL)
    {
        volume_deref(volume);
        return ERRPTR(EACCES);
    }

    char* rootPath = strchr(parsedPath, VFS_NAME_SEPARATOR);
    if (rootPath == NULL)
    {
        rootPath = parsedPath + strlen(parsedPath);
    }

    file_t* file = volume->ops->open(volume, rootPath);
    if (file == NULL)
    {
        volume_deref(volume);
        return NULL;
    }

    return file;
}

uint64_t vfs_open2(const char* path, file_t* files[2])
{
    char parsedPath[MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return ERROR(EPATH);
    }

    volume_t* volume = volume_get(parsedPath);
    if (volume == NULL)
    {
        return ERROR(EPATH);
    }

    if (volume->ops->open2 == NULL)
    {
        volume_deref(volume);
        return ERROR(EACCES);
    }

    char* rootPath = strchr(parsedPath, VFS_NAME_SEPARATOR);
    if (rootPath == NULL)
    {
        rootPath = parsedPath + strlen(parsedPath);
    }

    uint64_t result = volume->ops->open2(volume, rootPath, files);
    if (result == ERR)
    {
        volume_deref(volume);
        return ERR;
    }

    return result;
}

uint64_t vfs_stat(const char* path, stat_t* buffer)
{
    char parsedPath[MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return ERROR(EPATH);
    }

    volume_t* volume = volume_get(parsedPath);
    if (volume == NULL)
    {
        return ERROR(EPATH);
    }

    if (volume->ops->stat == NULL)
    {
        volume_deref(volume);
        return ERROR(EACCES);
    }

    char* rootPath = strchr(parsedPath, VFS_NAME_SEPARATOR);
    if (rootPath == NULL)
    {
        rootPath = parsedPath + strlen(parsedPath);
    }

    uint64_t result = volume->ops->stat(volume, rootPath, buffer);
    volume_deref(volume);
    return result;
}

uint64_t vfs_listdir(const char* path, dir_entry_t* entries, uint64_t amount)
{
    char parsedPath[MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return ERROR(EPATH);
    }

    volume_t* volume = volume_get(parsedPath);
    if (volume == NULL)
    {
        return ERROR(EPATH);
    }

    if (volume->ops->listdir == NULL)
    {
        volume_deref(volume);
        return ERROR(EACCES);
    }

    char* rootPath = strchr(parsedPath, VFS_NAME_SEPARATOR);
    if (rootPath == NULL)
    {
        rootPath = parsedPath + strlen(parsedPath);
    }

    uint64_t result = volume->ops->listdir(volume, rootPath, entries, amount);
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
        return ERROR(EACCES);
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
        return ERROR(EACCES);
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
        return ERROR(EACCES);
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
        return ERROR(EACCES);
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
        return ERROR(EACCES);
    }
    return file->ops->flush(file, buffer, size, rect);
}

void* vfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    if (file->resource != NULL && atomic_load(&file->resource->hidden))
    {
        return ERRPTR(ENORES);
    }
    if (file->ops->mmap == NULL)
    {
        return ERRPTR(EACCES);
    }
    return file->ops->mmap(file, address, length, prot);
}

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, nsec_t timeout)
{
    uint64_t currentTime = systime_uptime();
    uint64_t deadline = timeout == NEVER ? NEVER : currentTime + timeout;

    if (amount > CONFIG_MAX_BLOCKERS_PER_THREAD)
    {
        return ERROR(EBLOCKLIMIT);
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        if (files[i].file->resource != NULL && atomic_load(&files[i].file->resource->hidden))
        {
            return ERROR(ENORES);
        }
        if (files[i].file->ops->poll == NULL)
        {
            return ERROR(EACCES);
        }
        files[i].occurred = 0;
    }

    uint64_t events = 0;
    wait_queue_t* waitQueues[CONFIG_MAX_BLOCKERS_PER_THREAD];
    for (uint64_t i = 0; i < amount; i++)
    {
        waitQueues[i] = files[i].file->ops->poll(files[i].file, &files[i]);
        if (waitQueues[i] == NULL)
        {
            return ERR;
        }
    }

    while (true)
    {
        currentTime = systime_uptime();

        if (timeout != NEVER && currentTime >= deadline)
        {
            break;
        }

        events = 0;
        for (uint64_t i = 0; i < amount; i++)
        {
            if (files[i].file->ops->poll(files[i].file, &files[i]) == NULL)
            {
                return ERR;
            }

            if ((files[i].occurred & files[i].requested) != 0)
            {
                events++;
            }
        }

        if (events != 0)
        {
            break;
        }

        nsec_t remainingTime = deadline == NEVER ? NEVER : deadline - currentTime;
        waitsys_block_many(waitQueues, amount, remainingTime);
    }

    return events;
}

const char* vfs_basename(const char* path)
{
    const char* base = strrchr(path, VFS_NAME_SEPARATOR);
    return base != NULL ? base + 1 : path;
}

uint64_t vfs_parent_dir(char* dest, const char* src)
{
    const char* end = strrchr(src, VFS_NAME_SEPARATOR);
    if (end == NULL)
    {
        return ERR;
    }

    strncpy(dest, src, end - src);
    dest[end - src] = '\0';

    return 0;
}

const char* name_first(const char* path)
{
    if (path[0] == '\0')
    {
        return NULL;
    }
    else if (path[0] == VFS_NAME_SEPARATOR)
    {
        if (path[1] == '\0')
        {
            return NULL;
        }

        return path + 1;
    }

    return path;
}

const char* name_next(const char* path)
{
    const char* base = strchr(path, VFS_NAME_SEPARATOR);
    return base != NULL ? base + 1 : NULL;
}

uint64_t name_length(const char* name)
{
    for (uint64_t i = 0; i < MAX_PATH - 1; i++)
    {
        if (VFS_END_OF_NAME(name[i]))
        {
            return i;
        }
    }

    return MAX_PATH - 1;
}

void name_copy(char* dest, const char* src)
{
    for (uint64_t i = 0; i < MAX_NAME - 1; i++)
    {
        if (VFS_END_OF_NAME(src[i]))
        {
            dest[i] = '\0';
            return;
        }
        else
        {
            dest[i] = src[i];
        }
    }
    dest[MAX_NAME - 1] = '\0';
}

bool name_compare(const char* a, const char* b)
{
    for (uint64_t i = 0; i < MAX_PATH; i++)
    {
        if (VFS_END_OF_NAME(a[i]))
        {
            return VFS_END_OF_NAME(b[i]);
        }
        if (a[i] != b[i])
        {
            return false;
        }
    }

    return false;
}

bool name_valid(const char* name)
{
    uint64_t length = name_length(name);
    for (uint64_t i = 0; i < length; i++)
    {
        if (!VFS_VALID_CHAR(name[i]))
        {
            return false;
        }
    }

    return true;
}

bool label_compare(const char* a, const char* b)
{
    for (uint64_t i = 0; i < MAX_PATH; i++)
    {
        if (VFS_END_OF_LABEL(a[i]))
        {
            return VFS_END_OF_LABEL(b[i]);
        }
        if (a[i] != b[i])
        {
            return false;
        }
    }

    return false;
}

const char* dir_name_first(const char* path)
{
    if (path[0] == VFS_NAME_SEPARATOR)
    {
        path++;
    }

    if (strchr(path, VFS_NAME_SEPARATOR) == NULL)
    {
        return NULL;
    }
    else
    {
        return path;
    }
}

const char* dir_name_next(const char* path)
{
    const char* next = strchr(path, VFS_NAME_SEPARATOR);
    if (next == NULL)
    {
        return NULL;
    }
    else
    {
        next += 1;
        if (strchr(next, VFS_NAME_SEPARATOR) != NULL)
        {
            return next;
        }
        else
        {
            return NULL;
        }
    }
}

void dir_entry_push(dir_entry_t* entries, uint64_t amount, uint64_t* index, uint64_t* total, dir_entry_t* entry)
{
    if (*index < amount)
    {
        entries[(*index)++] = *entry;
    }

    (*total)++;
}
