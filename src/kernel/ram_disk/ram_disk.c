#include "ram_disk.h"

#include "vfs/vfs.h"
#include "tty/tty.h"
#include "scheduler/scheduler.h"
/*
uint64_t ram_disk_open(Disk* disk, File* out, const char* path, uint64_t flags)
{    
    if (((flags & O_READ) && disk->read == 0) ||
        ((flags & O_WRITE) && disk->write == 0))
    {
        scheduler_thread()->error = EACCES;
        return ERROR;
    }

    RamDirectory* directory = ram_disk_traverse(disk, path);
    if (directory == 0)
    {
        scheduler_thread()->error = EPATH;
        return ERROR;
    }

    const char* filename = vfs_utils_basename(path);
    if (filename == 0)
    {
        scheduler_thread()->error = EPATH;
        return ERROR;
    }

    RamFile* file = ram_directory_find_file(directory, filename);
    if (file == 0)
    {
        scheduler_thread()->error = ENAME;
        return ERROR;
    }

    (*out) = file_new(disk, file, flags);

    return 0;
}*/

void ram_disk_init(RamDirectory* root)
{
    tty_start_message("Ram Disk initializing");

    Disk* disk = disk_new("ram", root);
    if (disk == 0)
    {
        tty_print("Failed to create ram disk");
        tty_end_message(TTY_MESSAGE_ER);
    }

    //disk->open = ram_disk_open;

    if (vfs_mount(disk) == ERROR)
    {
        tty_print("Failed to mount ram disk");
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}