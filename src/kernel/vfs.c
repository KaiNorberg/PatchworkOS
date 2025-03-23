#include "vfs.h"

#include "lock.h"
#include "sched.h"
#include "time.h"
#include "waitsys.h"
#include "vfs_context.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <waitsys.h>

static list_t volumes;
static lock_t volumesLock;

static blocker_t pollBlocker;

// TODO: Improve file path parsing.

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
    LIST_FOR_EACH(volume, &volumes)
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

// TODO: Path parsing is completly incomprehensible.

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

    blocker_init(&pollBlocker);
}

uint64_t vfs_attach_simple(const char* label, const volume_ops_t* ops)
{
    if (strlen(label) >= MAX_NAME)
    {
        return ERROR(EINVAL);
    }
    LOCK_DEFER(&volumesLock);

    volume_t* volume;
    LIST_FOR_EACH(volume, &volumes)
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

    list_push(&volumes, volume);
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
    LIST_FOR_EACH(volume, &volumes)
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

    list_remove(volume);
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

static bool vfs_poll_condition(uint64_t* events, poll_file_t* files, uint64_t amount)
{
    *events = 0;
    for (uint64_t i = 0; i < amount; i++)
    {
        if (files[i].file->ops->poll(files[i].file, &files[i]) == NULL)
        {
            *events = ERR;
            return true;
        }

        if ((files[i].occurred & files[i].requested) != 0)
        {
            (*events)++;
        }
    }

    return *events != 0;
}

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, nsec_t timeout)
{    
    uint64_t deadline = timeout == NEVER ? NEVER : timeout + time_uptime();

    if (amount > CONFIG_MAX_BLOCKERS_PER_THREAD)
    {
        return ERROR(EBLOCKLIMIT);
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        if (files[i].file->ops->poll == NULL)
        {
            return ERROR(EACCES);
        }
        files[i].occurred = 0;
    }

    uint64_t events = 0;
    blocker_t* blockers[CONFIG_MAX_BLOCKERS_PER_THREAD];
    for (uint64_t i = 0; i < amount; i++)
    {
        blockers[i] = files[i].file->ops->poll(files[i].file, &files[i]);
        if (blockers[i] == NULL)
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
        return events;
    }

    events = 0;
    while (!vfs_poll_condition(&events, files, amount) && events != ERR && deadline > time_uptime())
    {
        blocker_block_many(blockers, amount, NEVER);
    }

    return events;
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
