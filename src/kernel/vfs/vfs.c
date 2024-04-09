#include "vfs.h"

#include <string.h>
#include <errno.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "sched/sched.h"
#include "debug/debug.h"
#include "vfs/utils/utils.h"

struct
{
    Drive* drives[VFS_LETTER_AMOUNT];
    Lock lock;
} driveTable;

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

static Drive* drive_get(char letter)
{
    if (!VFS_VALID_LETTER(letter))
    {
        return NULL;
    }
    LOCK_GUARD(driveTable.lock);

    uint64_t index = letter - VFS_LETTER_BASE;
    Drive* drive = driveTable.drives[index];
    if (drive == NULL)
    {
        return NULL;
    }

    atomic_fetch_add(&drive->ref, 1);
    return drive;
}

static void drive_put(Drive* drive)
{
    if (atomic_fetch_sub(&drive->ref, 1) <= 1)
    {
        debug_panic("Drive unmounting not implemented");
    }
}

static File* file_get(uint64_t fd)
{
    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(context->lock);

    if (context->files[fd] == NULL)
    {
        return NULL;
    }

    File* file = context->files[fd];
    atomic_fetch_add(&file->ref, 1);
    return file;
}

static void file_put(File* file)
{
    if (atomic_fetch_sub(&file->ref, 1) <= 1)
    {
        if (file->drive->fs->cleanup != NULL)
        {
            file->drive->fs->cleanup(file);
        }
        drive_put(file->drive);
        kfree(file);
    }
}

File* file_new(Drive* drive, void* context)
{
    File* file = kmalloc(sizeof(File));
    file->internal = context;
    file->drive = drive;
    file->position = 0;
    atomic_init(&file->ref, 1);

    return file;
}

void vfs_context_init(VfsContext* context)
{
    memset(context, 0, sizeof(VfsContext));
    strcpy(context->workDir, "A:/");
    context->lock = lock_create();
}

void vfs_context_cleanup(VfsContext* context)
{
    for (uint64_t i = 0; i < CONFIG_FILE_AMOUNT; i++)
    {    
        File* file = context->files[i];
        if (file != NULL)
        {
            file_put(file);
        }
    }
}

void vfs_init()
{
    tty_start_message("VFS initializing");

    memset(driveTable.drives, 0, sizeof(Drive*) * VFS_LETTER_AMOUNT);
    driveTable.lock = lock_create();

    tty_end_message(TTY_MESSAGE_OK);
}

uint64_t vfs_mount(char letter, Filesystem* fs, void* internal)
{
    if (!VFS_VALID_LETTER(letter))
    {
        return ERROR(ELETTER);
    }
    LOCK_GUARD(driveTable.lock);

    uint64_t index = letter - VFS_LETTER_BASE;
    if (driveTable.drives[index] != NULL)
    {
        return ERROR(EEXIST);
    }

    Drive* drive = kmalloc(sizeof(Drive));
    drive->internal = internal;
    drive->fs = fs;
    atomic_init(&drive->ref, 1);
    driveTable.drives[index] = drive;

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
    LOCK_GUARD(context->lock);

    strcpy(context->workDir, parsedPath);
    return 0;
}

uint64_t vfs_open(const char* path)
{
    char parsedPath[CONFIG_MAX_PATH];
    if (vfs_parse_path(parsedPath, path) == ERR)
    {
        return ERROR(EPATH);
    }

    Drive* drive = drive_get(parsedPath[0]);
    if (drive == NULL)
    {
        return ERROR(EPATH);
    }

    if (drive->fs->open == NULL)
    {
        drive_put(drive);
        return ERROR(EACCES);
    }

    //Drive reference is passed to file.
    File* file = drive->fs->open(drive, parsedPath + 2);
    if (file == NULL)
    {
        drive_put(drive);
        return ERR;
    }

    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(context->lock);

    for (uint64_t fd = 0; fd < CONFIG_FILE_AMOUNT; fd++)
    {
        if (context->files[fd] == NULL)
        {
            context->files[fd] = file;
            return fd;
        }
    }

    return ERROR(EMFILE);
}

uint64_t vfs_close(uint64_t fd)
{
    VfsContext* context = &sched_process()->vfsContext;
    LOCK_GUARD(context->lock);

    if (fd >= CONFIG_FILE_AMOUNT || context->files[fd] == NULL)
    {
        return ERROR(EBADF);
    }

    File* file = context->files[fd];
    context->files[fd] = NULL;
    file_put(file);

    return 0;
}

uint64_t vfs_read(uint64_t fd, void* buffer, uint64_t count)
{    
    File* file = file_get(fd);
    if (file == NULL)
    {
        return ERROR(EBADF);
    }

    if (file->drive->fs->read == NULL)
    {
        file_put(file);
        return ERROR(EACCES);
    }

    uint64_t result = file->drive->fs->read(file, buffer, count);
    file_put(file);
    return result;
}

uint64_t vfs_write(uint64_t fd, const void* buffer, uint64_t count)
{    
    File* file = file_get(fd);
    if (file == NULL)
    {
        return ERROR(EBADF);
    }

    if (file->drive->fs->write == NULL)
    {
        file_put(file);
        return ERROR(EACCES);
    }

    uint64_t result = file->drive->fs->write(file, buffer, count);
    file_put(file);
    return result;
}

uint64_t vfs_seek(uint64_t fd, int64_t offset, uint8_t origin)
{
    File* file = file_get(fd);
    if (file == NULL)
    {
        return ERROR(EBADF);
    }

    if (file->drive->fs->seek == NULL)
    {
        file_put(file);
        return ERROR(EACCES);
    }

    uint64_t result = file->drive->fs->seek(file, offset, origin);
    file_put(file);
    return result;
}