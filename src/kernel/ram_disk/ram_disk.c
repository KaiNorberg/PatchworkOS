#include "ram_disk.h"

#include <string.h>

#include "vfs/vfs.h"
#include "vfs/utils/utils.h"
#include "tty/tty.h"
#include "sched/sched.h"
#include "utils/utils.h"

static Filesystem ramfs;

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
    RamDir* dir = disk->fs->context;
    const char* dirname = vfs_first_dir(path);
    while (dirname != NULL)
    {
        dir = ram_dir_find_dir(dir, dirname);
        if (dir == NULL)
        {
            return NULL;
        }

        dirname = vfs_next_dir(dirname);
    }

    return dir;
}

uint64_t ram_disk_open(Disk* disk, File* file, const char* path)
{
    RamDir* ramDir = ram_disk_traverse(disk, path);
    if (ramDir == NULL)
    {
        return ERROR(EPATH);
    }

    const char* filename = vfs_basename(path);
    if (filename == NULL)
    {
        return ERROR(EPATH);
    }

    RamFile* ramFile = ram_dir_find_file(ramDir, filename);
    if (ramFile == NULL)
    {
        return ERROR(ENAME);
    }

    file->context = ramFile;

    return 0;
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

    return file->position;
}

void ram_disk_init(RamDir* root)
{
    tty_start_message("Ram Disk initializing");

    strcpy(ramfs.name, "ramfs");
    ramfs.context = root;
    ramfs.diskFuncs.open = ram_disk_open;
    ramfs.fileFuncs.cleanup = NULL;
    ramfs.fileFuncs.read = ram_disk_read;
    ramfs.fileFuncs.write = NULL;
    ramfs.fileFuncs.seek = ram_disk_seek;

    if (vfs_mount("ram", NULL, &ramfs) == ERR)
    {
        tty_print("Failed to mount ram disk");
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}