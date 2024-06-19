#include "vfs.h"

#include "debug.h"
#include "lock.h"
#include "sched.h"
#include "splash.h"
#include "time.h"
#include "vfs_context.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static list_t volumes;
static lock_t volumeLock;

// TODO: Improve file path parsing.

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
        if (name == NULL)
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
    LOCK_GUARD(&context->lock);

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
    LOCK_GUARD(&volumeLock);

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

file_t* file_new(void)
{
    file_t* file = malloc(sizeof(file_t));
    atomic_init(&file->ref, 1);
    file->volume = NULL;
    file->position = 0;
    file->internal = NULL;
    file->cleanup = NULL;
    memset(&file->ops, 0, sizeof(file_ops_t));

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
        if (file->cleanup != NULL)
        {
            file->cleanup(file);
        }
        if (file->volume != NULL)
        {
            volume_deref(file->volume);
        }
        free(file);
    }
}

/*void test_path(const char* path)
{
    tty_print(path);
    tty_print(" => ");
    char parsedPath[MAX_PATH];
    parsedPath[0] = '\0';
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        tty_print("ERR\n");
        return;
    }
    tty_print(parsedPath);
    tty_print("\n");
}*/

void vfs_init(void)
{
    list_init(&volumes);
    lock_init(&volumeLock);

    /*tty_print("\n");

    vfs_chdir("vol:/wd1/wd2");

    test_path("sys:/test1/test2/test3");
    test_path("sys:/test1/../test3");
    test_path("sys:/../../test3");
    test_path("sys:/test1/test2/test3/../..");
    test_path("sys:/test1/../test3/..");

    tty_print("\n");

    test_path("/test1/test2/test3");
    test_path("/test1/../test3");
    test_path("/../../test3");
    test_path("/test1/test2/test3/../..");
    test_path("/test1/../test3/..");

    tty_print("\n");

    test_path("test1/test2/test3");
    test_path("test1/../test3");
    test_path("../../test3");
    test_path("test1/test2/test3/../..");
    test_path("test1/../test3/..");

    tty_print("\n");

    test_path("../test3");
    test_path(".");
    test_path("sys:/test1/test2");
    test_path("sys:test1/test2");
    test_path("/test1/test2/../../test3");

    while (1);*/
}

file_t* vfs_open(const char* path)
{
    char parsedPath[MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return NULLPTR(EPATH);
    }

    volume_t* volume = volume_get(parsedPath);
    if (volume == NULL)
    {
        return NULLPTR(EPATH);
    }

    if (volume->open == NULL)
    {
        volume_deref(volume);
        return NULLPTR(EACCES);
    }

    // volume_t reference is passed to file.
    file_t* file = file_new();
    file->volume = volume;

    char* rootPath = strchr(parsedPath, VFS_NAME_SEPARATOR);
    if (rootPath == NULL || volume->open(volume, file, rootPath) == ERR)
    {
        file_deref(file);
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

    if (volume->stat == NULL)
    {
        volume_deref(volume);
        return ERROR(EACCES);
    }

    char* rootPath = strchr(parsedPath, VFS_NAME_SEPARATOR);
    if (rootPath == NULL)
    {
        volume_deref(volume);
        return ERR;
    }

    uint64_t result = volume->stat(volume, rootPath, buffer);
    volume_deref(volume);
    return result;
}

uint64_t vfs_mount(const char* label, fs_t* fs)
{
    if (strlen(label) >= CONFIG_MAX_LABEL)
    {
        return ERROR(EINVAL);
    }
    LOCK_GUARD(&volumeLock);

    volume_t* volume;
    LIST_FOR_EACH(volume, &volumes)
    {
        if (name_compare(volume->label, label))
        {
            return ERROR(EEXIST);
        }
    }

    volume = malloc(sizeof(volume_t));
    memset(volume, 0, sizeof(volume_t));
    strcpy(volume->label, label);
    volume->fs = fs;
    atomic_init(&volume->ref, 1);

    if (fs->mount(volume) == ERR)
    {
        free(volume);
        return ERR;
    }

    list_push(&volumes, volume);
    return 0;
}

uint64_t vfs_unmount(const char* label)
{
    LOCK_GUARD(&volumeLock);

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

    if (volume->unmount == NULL)
    {
        return ERROR(EACCES);
    }

    if (volume->unmount(volume) == ERR)
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
    LOCK_GUARD(&context->lock);

    strcpy(context->cwd, parsedPath);
    return 0;
}

static bool vfs_poll_condition(uint64_t* events, poll_file_t* files, uint64_t amount)
{
    *events = 0;
    for (uint64_t i = 0; i < amount; i++)
    {
        poll_file_t* file = &files[i];

        if (file->requested & POLL_READ && (file->file->ops.read_avail != NULL && file->file->ops.read_avail(file->file)))
        {
            file->occurred = POLL_READ;
            (*events)++;
        }
        if (file->requested & POLL_WRITE && (file->file->ops.write_avail != NULL && file->file->ops.write_avail(file->file)))
        {
            file->occurred = POLL_WRITE;
            (*events)++;
        }

        file++;
    }

    return *events != 0;
}

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, uint64_t timeout)
{
    uint64_t events = 0;
    if (SCHED_WAIT(vfs_poll_condition(&events, files, amount), timeout) == ERR)
    {
        return ERR;
    }

    return events;
}
