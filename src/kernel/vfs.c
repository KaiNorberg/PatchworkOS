#include "vfs.h"

#include <string.h>
#include <errno.h>

#include "tty.h"
#include "heap.h"
#include "lock.h"
#include "sched.h"
#include "debug.h"
#include "time.h"
#include "vfs_context.h"

static List volumes;
static Lock volumeLock;

static uint64_t vfs_make_canonical(char* start, char* out, const char* path)
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
            out--;
            while (*out != VFS_SEPARATOR)
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
                if (!VFS_VALID_CHAR(*ptr) || (uint64_t)(out - start) >= CONFIG_MAX_PATH - 2)
                {
                    return ERR;
                } 

                out++;
                *out = *ptr;
                ptr++;
            }

            out++;
            out[0] = VFS_SEPARATOR;
            out[1] = '\0';
        }            
        
        name = vfs_next_name(name);
        if (name == NULL)
        {                    
            if (*out == VFS_SEPARATOR)
            {
                *out = '\0';
            }
            return 0;
        }
    }
}

static uint64_t vfs_parse_path(char* out, const char* path)
{
    if (path[0] == VFS_DRIVE_ACCESSOR) //Absolute path
    {       
        out[0] = VFS_DRIVE_ACCESSOR;
        out[1] = '\0';
        return vfs_make_canonical(out, out, path + 1);
    }
    
    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(&context->lock);
    
    if (path[0] == VFS_SEPARATOR) //Root path
    {
        uint64_t volumeLength = vfs_name_length(context->cwd + 1);
        out[0] = VFS_DRIVE_ACCESSOR;
        memcpy(out + 1, context->cwd + 1, volumeLength);
        return vfs_make_canonical(out, out + volumeLength, path);
    }
    else if (VFS_VALID_CHAR(path[0])) //Relative path
    {
        uint64_t i = 0;
        while (context->cwd[i] != '\0')
        {
            out[i] = context->cwd[i];
            i++;
        }
        out[i] = VFS_SEPARATOR;
        out[i + 1] = '\0';

        return vfs_make_canonical(out, out + i, path);
    }

    return ERR;
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

static Volume* volume_get(const char* label)
{
    LOCK_GUARD(&volumeLock);

    Volume* volume;
    LIST_FOR_EACH(volume, &volumes)
    {
        if (vfs_compare_names(volume->label, label))
        {
            return volume_ref(volume);
        }
    }

    return NULL;
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

void vfs_init(void)
{
    tty_start_message("VFS initializing");

    list_init(&volumes);
    lock_init(&volumeLock);

    tty_end_message(TTY_MESSAGE_OK);
}

File* vfs_open(const char* path)
{
    char parsedPath[CONFIG_MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return NULLPTR(EPATH);
    }

    Volume* volume = volume_get(parsedPath + 1);
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

    char* volumePath = strchr(parsedPath, VFS_SEPARATOR);
    if (volumePath == NULL || volume->open(volume, file, volumePath) == ERR)
    {
        file_deref(file);
        return NULL;
    }

    return file;
}

static bool vfs_poll_callback(void* context)
{
    PollFile* file = context;
    while (file->file != NULL)
    {
        if (file->requested & POLL_READ &&
            (file->file->methods.read_avail != NULL && file->file->methods.read_avail(file->file)))
        {
            file->occurred = POLL_READ;
            return true;
        }
        if (file->requested & POLL_WRITE &&
            (file->file->methods.write_avail != NULL && file->file->methods.write_avail(file->file)))
        {
            file->occurred = POLL_WRITE;
            return true;
        }

        file++;
    }

    return false;
}

uint64_t vfs_poll(PollFile* files, uint64_t timeout)
{
    Blocker blocker =
    {
        .context = files,
        .callback = vfs_poll_callback,
        .deadline = timeout == UINT64_MAX ? UINT64_MAX : timeout + time_nanoseconds()
    };
    sched_block(blocker);

    return 0;
}

uint64_t vfs_mount(const char* label, Filesystem* fs)
{
    if (strlen(label) >= CONFIG_MAX_LABEL)
    {
        return ERROR(EINVAL);
    }

    LOCK_GUARD(&volumeLock);

    Volume* volume;
    LIST_FOR_EACH(volume, &volumes)
    {
        if (vfs_compare_names(volume->label, label))
        {
            return ERROR(EEXIST);
        }
    }

    volume = kmalloc(sizeof(Volume));
    memset(volume, 0, sizeof(Volume));
    strcpy(volume->label, label);
    volume->fs = fs;
    atomic_init(&volume->ref, 1);

    if (fs->mount(volume) == ERR)
    {
        kfree(volume);
        return ERR;
    }

    list_push(&volumes, volume);
    return 0;
}

uint64_t vfs_unmount(const char* label)
{
    LOCK_GUARD(&volumeLock);

    Volume* volume;
    bool found = false;
    LIST_FOR_EACH(volume, &volumes)
    {
        if (vfs_compare_names(volume->label, label))
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
    kfree(volume);
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