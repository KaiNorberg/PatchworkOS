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

static Volume volumes[VFS_LETTER_AMOUNT];

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
                if (!VFS_VALID_CHAR(*ptr) || (uint64_t)(out - start) >= CONFIG_MAX_PATH - 2)
                {
                    return ERR;
                } 

                out++;
                *out = *ptr;
                ptr++;
            }

            out++;
            out[0] = VFS_NAME_SEPARATOR;
            out[1] = '\0';
        }            
        
        name = vfs_next_name(name);
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
    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(&context->lock);
    
    if (path[0] == '\0')
    {
        return ERR;
    }

    if (path[1] == VFS_VOLUME_SEPARATOR) //Absolute path
    {
        if (!VFS_VALID_LETTER(path[0]))
        {
            return ERR;
        }

        out[0] = path[0];
        out[1] = VFS_VOLUME_SEPARATOR;
        out[2] = VFS_NAME_SEPARATOR;
        out[4] = '\0';

        return vfs_make_canonical(out + 2, out + 2, path + 3);
    }
    else if (path[0] == VFS_NAME_SEPARATOR) //Root path
    {
        out[0] = context->cwd[0];
        out[1] = VFS_VOLUME_SEPARATOR;
        out[2] = '\0';

        return vfs_make_canonical(out + 1, out + 1, path);
    }
    else if (VFS_VALID_CHAR(path[0]))
    {
        uint64_t cwdLength = strlen(context->cwd);
        if (cwdLength >= CONFIG_MAX_PATH - 2)
        {
            return ERR;
        }

        memcpy(out, context->cwd, cwdLength);
        out[cwdLength] = VFS_NAME_SEPARATOR;
        out[cwdLength + 1] = '\0';

        return vfs_make_canonical(out + 2, out + cwdLength, path);
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

void test_path(const char* path)
{
    tty_print(path);
    tty_print(" => ");
    char parsedPath[CONFIG_MAX_PATH];
    parsedPath[0] = '\0';
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        tty_print("ERR\n");
        return;
    }
    tty_print(parsedPath);
    tty_print("\n");
}

void vfs_init(void)
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

    char* volumePath = strchr(parsedPath, VFS_NAME_SEPARATOR);
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