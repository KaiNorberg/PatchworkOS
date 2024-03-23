#include "vfs.h"

#include <string.h>
#include <errno.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "array/array.h"
#include "scheduler/scheduler.h"
#include "vfs/utils/utils.h"

static Array* disks;

static uint64_t vfs_disk_find_callback(void* element, void* context)
{
    Disk* disk = element;
    const char* name = context;

    if (vfs_compare_names(disk->name, name) == 0)
    {
        return ARRAY_FIND_FOUND;
    }
    else
    {
        return ARRAY_FIND_NOT_FOUND;
    }
}

static inline Disk* vfs_disk_find(const char* name)
{
    return array_find(disks, vfs_disk_find_callback, (void*)name);
}

Disk* disk_new(const char* name, void* context)
{
    if (!vfs_valid_name(name))
    {
        return 0;
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
    if (vfs_disk_find(disk->name) != 0)
    {
        scheduler_thread()->errno = EEXIST;
        return ERROR;
    }

    array_push(disks, disk);
    return 0;
}

uint64_t vfs_open(const char* path, uint64_t flags)
{
    /*if (!vfs_valid_path(path))
    {
        scheduler_thread()->errno = EPATH;
        return ERROR;
    }*/

    return 0;
}