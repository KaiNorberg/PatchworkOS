#include "vfs.h"

#include <string.h>
#include <errno.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "sched/sched.h"
#include "debug/debug.h"
#include "vfs/context/context.h"
#include "vfs/utils/utils.h"

struct
{
    Volume* volumes[VFS_LETTER_AMOUNT];
    Lock lock;
} volumeTable;

//TODO: Clean up vfs filepath parsing

static uint64_t vfs_make_path_canonical(char* start, char* out, const char* path, uint64_t count)
{
    const char* name = path;
    while (true)
    {
        if (vfs_compare_names(name, "."))
        {
            //Do nothing
        }
        else if (vfs_compare_names(name, ".."))
        {            
            out = strrchr(start, VFS_NAME_SEPARATOR);
            if (out == NULL)
            {
                return ERR;
            }
            *out = '\0';
        }
        else
        {
            if ((uint64_t)(out - start) >= count)
            {
                return ERR;
            }
            *out++ = VFS_NAME_SEPARATOR;

            for (const char* ptr = name; !VFS_END_OF_NAME(*ptr); ptr++)
            {
                if (!VFS_VALID_CHAR(*ptr) || (uint64_t)(out - start) >= count)
                {
                    return ERR;
                } 
                *out++ = *ptr;
            }
        }

        const char* next = vfs_next_name(name);
        if (next == NULL)
        {
            *out = '\0';
            return 0;
        }

        name = next;
    }
}

static uint64_t vfs_parse_path(char* out, const char* path)
{    
    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(context->lock);

    if (path[0] != '\0' && path[1] == VFS_DRIVE_SEPARATOR) //absolute path
    {
        if (!VFS_VALID_LETTER(path[0]) || path[2] != VFS_NAME_SEPARATOR)
        {
            return ERR;
        }

        out[0] = path[0];
        out[1] = VFS_DRIVE_SEPARATOR;

        return vfs_make_path_canonical(out, out + 2, path + 3, CONFIG_MAX_PATH - 3);
    }
    else if (path[0] == VFS_NAME_SEPARATOR) //root path
    {
        out[0] = context->workDir[0];
        out[1] = VFS_DRIVE_SEPARATOR;

        return vfs_make_path_canonical(out, out + 2, path + 1, CONFIG_MAX_PATH - 3);
    }
    else //relative path
    {
        uint64_t workLength = strlen(context->workDir);
        memcpy(out, context->workDir, workLength);
        return vfs_make_path_canonical(out, out + workLength, path, CONFIG_MAX_PATH - workLength);
    }
}

static Volume* volume_table_get(char letter)
{
    if (!VFS_VALID_LETTER(letter))
    {
        return NULL;
    }

    LOCK_GUARD(volumeTable.lock);
    Volume* volume = volumeTable.volumes[letter - VFS_LETTER_BASE];
    if (volume == NULL)
    {
        return NULL;
    }

    return volume_ref(volume);
}

void vfs_init()
{
    tty_start_message("VFS initializing");

    memset(volumeTable.volumes, 0, sizeof(Volume*) * VFS_LETTER_AMOUNT);
    lock_init(&volumeTable.lock);

    tty_end_message(TTY_MESSAGE_OK);
}

File* vfs_open(const char* path)
{
    char parsedPath[CONFIG_MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return NULLPTR(EPATH);
    }

    Volume* volume = volume_table_get(parsedPath[0]);
    if (volume == NULL)
    {
        return NULLPTR(EPATH);
    }

    if (volume->open == NULL)
    {
        volume_deref(volume);
        return NULLPTR(EACCES);
    }

    //Volume reference is passed to file.
    File* file = volume->open(volume, parsedPath + 2);
    if (file == NULL)
    {
        volume_deref(volume);
        return NULL;
    }

    return file;
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

uint64_t vfs_mount(char letter, Filesystem* fs)
{
    if (!VFS_VALID_LETTER(letter))
    {
        return ERROR(ELETTER);
    }
    LOCK_GUARD(volumeTable.lock);

    uint64_t index = letter - VFS_LETTER_BASE;
    if (volumeTable.volumes[index] != NULL)
    {
        return ERROR(EEXIST);
    }

    Volume* volume = fs->mount(fs);
    if (volume == NULL)
    {
        return ERR;
    }

    volumeTable.volumes[index] = volume;
    return 0;
}

uint64_t vfs_chdir(const char* path)
{    
    char parsedPath[CONFIG_MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return ERROR(EPATH);
    }

    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(context->lock);

    strcpy(context->workDir, parsedPath);
    return 0;
}