#include "ram_disk.h"

#include <string.h>

#include "vfs/vfs.h"
#include "vfs/utils/utils.h"
#include "tty/tty.h"
#include "sched/sched.h"
#include "utils/utils.h"

static inline RamFile* ram_dir_find_file(RamDir* dir, const char* filename)
{
    RamFile* file = dir->firstFile;
    while (file != NULL)
    {
        if (vfs_compare_names(file->name, filename))
        {
            return file;
        }
        else
        {
            file = file->next;
        }
    }

    return NULL;
}

static inline RamDir* ram_dir_find_dir(RamDir* dir, const char* dirname)
{
    RamDir* child = dir->firstChild;
    while (child != NULL)
    {
        if (vfs_compare_names(child->name, dirname))
        {
            return child;
        }
        else
        {
            child = child->next;
        }
    }

    return NULL;
}

static inline RamDir* ram_disk_traverse(Disk* disk, const char* path)
{
    RamDir* dir = disk->context;
    while (path != NULL)
    {
        dir = ram_dir_find_dir(dir, path);
        if (dir == NULL)
        {
            return NULL;
        }

        path = vfs_next_dir(path);
    }

    return dir;
}

fd_t ram_disk_open(Disk* disk, const char* path, uint8_t flags)
{
    RamDir* dir = ram_disk_traverse(disk, path);
    if (dir == NULL)
    {
        return ERROR(EPATH);
    }

    const char* filename = vfs_basename(path);
    if (filename == NULL)
    {
        return ERROR(EPATH);
    }

    RamFile* file = ram_dir_find_file(dir, filename);
    if (file == NULL)
    {
        return ERROR(ENAME);
    }

    fd_t fd = file_table_open(disk, flags, file);
    if (fd == ERR)
    {
        return ERROR(EMFILE);
    }

    return fd;
}

void ram_disk_cleanup(File* file)
{
    
}

uint64_t ram_disk_read(File* file, void* buffer, uint64_t count)
{
    RamFile const* ramFile = file->context;

    size_t pos = file->position;
    size_t readCount = pos <= ramFile->size ? MIN(count, ramFile->size - pos) : 0;
    atomic_fetch_add(&file->position, readCount);

    memcpy(buffer, ramFile->data + pos, readCount);
    return readCount;
}

uint64_t ram_disk_seek(File* file, int64_t offset, uint8_t origin)
{
    RamFile const* ramFile = file->context;

    switch (origin)
    {
    case SEEK_SET:
    {
        file->position = offset;
    }
    break;
    case SEEK_CUR:
    {
        file->position += offset;
    }
    break;
    case SEEK_END:
    {
        file->position = ramFile->size - offset;
    }
    break;
    }

    return 0;
}

void ram_disk_init(RamDir* root)
{
    tty_start_message("Ram Disk initializing");

    Disk* disk = disk_new("ram", root);
    if (disk == NULL)
    {
        tty_print("Failed to create ram disk");
        tty_end_message(TTY_MESSAGE_ER);
    }

    disk->open = ram_disk_open;
    disk->cleanup = ram_disk_cleanup;
    disk->read = ram_disk_read;
    disk->seek = ram_disk_seek;

    if (vfs_mount(disk) == ERR)
    {
        tty_print("Failed to mount ram disk");
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}