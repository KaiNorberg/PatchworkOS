#include "vfs.h"

#include <string.h>
#include <errno.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "array/array.h"
#include "sched/sched.h"
#include "debug/debug.h"
#include "vfs/utils/utils.h"

struct
{
    Drive* drives[VFS_LETTER_AMOUNT];
    Lock lock;
} driveTable;

//This is temporary
static bool vfs_valid_path(const char* path)
{
    if (!VFS_VALID_LETTER(path[0]) || path[1] != VFS_DRIVE_DELIMITER || path[2] != VFS_NAME_DELIMITER)
    {
        return false;
    }

    for (uint64_t i = 3; i < CONFIG_MAX_PATH; i++)
    {
        if (path[i] == '\0')
        {
            return true;
        }
        else if (path[i] == VFS_NAME_DELIMITER)
        {
            continue;
        }
        else if (!VFS_VALID_CHAR(path[i]))
        {
            return false;
        }
    }

    return false;
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
    FileTable* table = &sched_process()->fileTable;
    LOCK_GUARD(table->lock);

    if (table->files[fd] == NULL)
    {
        lock_release(&table->lock);
        return NULL;
    }

    File* file = table->files[fd];
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

void file_table_init(FileTable* table)
{
    memset(table, 0, sizeof(FileTable));
    table->lock = lock_create();
}

void file_table_cleanup(FileTable* table)
{
    for (uint64_t i = 0; i < CONFIG_FILE_AMOUNT; i++)
    {    
        File* file = table->files[i];
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

uint64_t vfs_mount(char letter, Filesystem* fs, void* context)
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
    drive->context = context;
    drive->fs = fs;
    drive->ref = 1;
    driveTable.drives[index] = drive;

    return 0;
}

uint64_t vfs_open(const char* path)
{
    if (!vfs_valid_path(path))
    {
        return ERROR(EPATH);
    }

    Drive* drive = drive_get(path[0]);
    if (drive == NULL)
    {
        return ERROR(EPATH);
    }

    if (drive->fs->open == NULL)
    {
        drive_put(drive);
        return ERROR(EACCES);
    }

    File* file = drive->fs->open(drive, path + 2);
    if (file == NULL)
    {
        drive_put(drive);
        return ERR;
    }

    FileTable* table = &sched_process()->fileTable;
    LOCK_GUARD(table->lock);

    for (uint64_t fd = 0; fd < CONFIG_FILE_AMOUNT; fd++)
    {
        if (table->files[fd] == NULL)
        {
            table->files[fd] = file;
            return fd;
        }
    }

    return ERROR(EMFILE);
}

uint64_t vfs_close(uint64_t fd)
{    
    FileTable* table = &sched_process()->fileTable;
    LOCK_GUARD(table->lock);

    if (fd >= CONFIG_FILE_AMOUNT || table->files[fd] == NULL)
    {
        return ERROR(EBADF);
    }

    File* file = table->files[fd];
    table->files[fd] = NULL;
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