#include "vfs.h"

#include <string.h>
#include <errno.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "lock/lock.h"
#include "sched/sched.h"
#include "debug/debug.h"
#include "vfs/context/context.h"
#include "vfs/utils/utils.h"

static Volume volumes[VFS_LETTER_AMOUNT];

//TODO: Improve vfs filepath parsing

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
    LOCK_GUARD(&context->lock);

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
        out[0] = context->cwd[0];
        out[1] = VFS_DRIVE_SEPARATOR;

        return vfs_make_path_canonical(out, out + 2, path + 1, CONFIG_MAX_PATH - 3);
    }
    else //relative path
    {
        uint64_t workLength = strlen(context->cwd);
        memcpy(out, context->cwd, workLength);
        return vfs_make_path_canonical(out, out + workLength, path, CONFIG_MAX_PATH - workLength);
    }
}

static Volume* volume_ref(Volume* volume)
{
    atomic_fetch_add(&volume->ref, 1);
    return volume;
}

static void volume_deref(Volume* volume)
{
    atomic_fetch_sub(&volume->ref, 1);
}

static Volume* volume_get(char letter)
{
    if (!VFS_VALID_LETTER(letter))
    {
        return NULL;
    }
    
    Volume* volume = &volumes[letter - VFS_LETTER_BASE];
    LOCK_GUARD(&volume->lock);

    if (atomic_load(&volume->ref) == 0)
    {
        return NULL;
    }

    return volume_ref(volume);
}

File* file_ref(File* file)
{
    atomic_fetch_add(&file->ref, 1);
    return file;
}

void file_deref(File* file)
{
    if (atomic_fetch_sub(&file->ref, 1) <= 1)
    {        
        if (file->cleanup != NULL)
        {
            file->cleanup(file);
        }
        volume_deref(file->volume);
        kfree(file);
    }
}

void vfs_init()
{
    tty_start_message("VFS initializing");

    memset(&volumes, 0, sizeof(Volume) * VFS_LETTER_AMOUNT);
    for (uint64_t i = 0; i < VFS_LETTER_AMOUNT; i++)
    {
        atomic_init(&volumes[i].ref, 0); //Not needed but good practice
        lock_init(&volumes[i].lock);
    }

    tty_end_message(TTY_MESSAGE_OK);
}

File* vfs_open(const char* path)
{
    char parsedPath[CONFIG_MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return NULLPTR(EPATH);
    }

    Volume* volume = volume_get(parsedPath[0]);
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
    File* file = kmalloc(sizeof(File));
    atomic_init(&file->ref, 1);
    file->volume = volume;
    file->position = 0;
    file->internal = NULL;
    file->cleanup = NULL;
    memset(&file->methods, 0, sizeof(FileMethods));

    if (volume->open(volume, file, parsedPath + 2) == ERR)
    {
        file_deref(file);
        return NULL;
    }

    return file;
}

uint64_t vfs_mount(char letter, Filesystem* fs)
{
    if (!VFS_VALID_LETTER(letter))
    {
        return ERROR(ELETTER);
    }

    Volume* volume = &volumes[letter - VFS_LETTER_BASE];
    LOCK_GUARD(&volume->lock);

    if (atomic_load(&volume->ref) != 0) 
    {
        return ERROR(EEXIST);
    }

    atomic_init(&volume->ref, 1);
    volume->fs = fs;

    if (fs->mount(volume) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t vfs_unmount(char letter)
{
    if (!VFS_VALID_LETTER(letter))
    {
        return ERROR(ELETTER);
    }

    Volume* volume = &volumes[letter - VFS_LETTER_BASE];
    LOCK_GUARD(&volume->lock);

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

    memset(volume, 0, sizeof(Volume));
    atomic_store(&volume->ref, 0);
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
    char parsedPath[CONFIG_MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return ERROR(EPATH);
    }

    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(&context->lock);

    strcpy(context->cwd, parsedPath);
    return 0;
}