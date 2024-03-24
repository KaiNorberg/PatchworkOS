#include "ram_disk.h"

#include "vfs/vfs.h"
#include "vfs/utils/utils.h"
#include "tty/tty.h"
#include "scheduler/scheduler.h"

/*static inline RamFile* ram_directory_find_file(RamDirectory* directory, const char* filename)
{
    RamFile* file = directory->firstFile;
    while (file != NULL)
    {
        if (vfs_compare_names(file->name, filename))
        {
            break;
        }
        else
        {
            file = file->next;
        }
    }

    return file;
}

static inline RamDirectory* ram_directory_find_directory(RamDirectory* directory, const char* dirname)
{
    RamDirectory* child = directory->firstChild;
    while (child != NULL)
    {
        if (vfs_compare_names(child->name, dirname))
        {
            break;
        }
        else
        {
            child = child->next;
        }
    }

    return child;
}

static inline RamDirectory* ram_disk_traverse(Disk* disk, const char* path)
{
    RamDirectory* directory = disk->context;
    while (path != NULL)
    {
        directory = ram_directory_find_directory(directory, path);
        if (directory == NULL)
        {
            return NULL;
        }

        path = vfs_next_dir(path);
    }

    return directory;
}

uint64_t ram_disk_open(Disk* disk, const char* path, uint64_t flags)
{        
    RamDirectory* directory = ram_disk_traverse(disk, path);
    if (directory == NULL)
    {
        scheduler_thread()->errno = EPATH;
        return ERROR;
    }

    const char* filename = vfs_basename(path);
    if (filename == NULL)
    {
        scheduler_thread()->errno = EPATH;
        return ERROR;
    }

    RamFile* file = ram_directory_find_file(directory, filename);
    if (file == NULL)
    {
        scheduler_thread()->errno = ENAME;
        return ERROR;
    }

    return file_table_open(disk, flags, file);
}

uint64_t ram_disk_close(Disk* disk, File* file)
{
    return 0;
}*/

void ram_disk_init(RamDirectory* root)
{
    /*tty_start_message("Ram Disk initializing");

    Disk* disk = disk_new("ram", root);
    if (disk == NULL)
    {
        tty_print("Failed to create ram disk");
        tty_end_message(TTY_MESSAGE_ER);
    }

    disk->open = ram_disk_open;
    disk->close = ram_disk_close;

    if (vfs_mount(disk) == ERROR)
    {
        tty_print("Failed to mount ram disk");
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);*/
}