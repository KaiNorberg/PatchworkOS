#include "vfs.h"

#include <string.h>
#include <errno.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "array/array.h"
#include "sched/sched.h"
#include "vfs/utils/utils.h"

static Array* disks;

static uint64_t vfs_find_disk_callback(void* element, void* context)
{
    Disk* disk = element;
    const char* name = context;

    if (vfs_compare_names(disk->name, name))
    {
        return ARRAY_FIND_FOUND;
    }
    else
    {
        return ARRAY_FIND_NOT_FOUND;
    }
}

static inline Disk* vfs_find_disk(const char* name)
{
    return array_find(disks, vfs_find_disk_callback, (void*)name);
}

Disk* disk_new(const char* name, void* context)
{
    if (!vfs_valid_name(name))
    {
        return NULL;
    }

    Disk* disk = kmalloc(sizeof(Disk));
    memset(disk, 0, sizeof(Disk));

    strcpy(disk->name, name);
    disk->context = context;

    return disk;
}

void vfs_init()
{
    tty_start_message("VFS initializing");

    disks = array_new();

    tty_end_message(TTY_MESSAGE_OK);
}

uint64_t vfs_mount(Disk* disk)
{
    if (vfs_find_disk(disk->name) != 0)
    {
        return ERROR(EEXIST);
    }

    array_push(disks, disk);
    return 0;
}

fd_t vfs_open(const char* path, uint8_t flags)
{
    if (!vfs_valid_path(path))
    {
        return ERROR(EPATH);
    }

    Disk* disk = vfs_find_disk(path);
    if (disk == NULL)
    {
        return ERROR(ENAME);
    }

    if ((disk->read == 0 && (flags & O_READ)) ||
        (disk->write == 0 && (flags & O_WRITE)))
    {
        return ERROR(EACCES);
    }

    path = strchr(path, VFS_DISK_DELIMITER) + 1;
    if (path[0] != VFS_NAME_DELIMITER)
    {
        return ERROR(EPATH);
    }
    
    return disk->open(disk, path + 1, flags);
}

uint64_t vfs_close(fd_t fd)
{
    if (file_table_close(fd) == ERR)
    {
        return ERROR(EBADF);
    }

    return 0;
}

uint64_t vfs_read(fd_t fd, void* buffer, uint64_t count)
{    
    File* file = file_table_get(fd);
    if (file == NULL)
    {
        return ERROR(EBADF);
    }

    uint64_t result;
    if (file->flags & O_READ)
    {
        result = file->disk->read(file, buffer, count);
    }
    else
    {
        result = ERROR(EACCES);
    }

    file_table_put(file);
    return result;
}

uint64_t vfs_write(fd_t fd, const void* buffer, uint64_t count)
{
    File* file = file_table_get(fd);
    if (file == NULL)
    {
        return ERROR(EBADF);
    }

    uint64_t result;
    if (file->flags & O_WRITE)
    {
        result = file->disk->write(file, buffer, count);
    }
    else
    {
        result = ERROR(EACCES);
    }

    file_table_put(file);
    return result;
}

uint64_t vfs_seek(fd_t fd, int64_t offset, uint8_t origin)
{
    File* file = file_table_get(fd);
    if (file == NULL)
    {
        return ERROR(EBADF);
    }

    uint64_t result;
    if (file->disk->seek != NULL)
    {
        result = file->disk->seek(file, offset, origin);
    }
    else
    {
        result = ERROR(EACCES);
    }

    file_table_put(file);
    return result;
}