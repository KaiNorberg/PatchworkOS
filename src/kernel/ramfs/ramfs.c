#include "ramfs.h"

#include <string.h>

#include "vfs/vfs.h"
#include "vfs/utils/utils.h"
#include "tty/tty.h"
#include "sched/sched.h"
#include "utils/utils.h"

static Filesystem ramfs;
static RamDir* root;

static RamFile* ram_dir_find_file(RamDir* dir, const char* filename)
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

static RamDir* ram_dir_find_dir(RamDir* dir, const char* dirname)
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

static RamDir* ramfs_traverse(const char* path)
{
    RamDir* dir = root;
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

static RamFile* ramfs_find_file(const char* path)
{
    RamDir* parent = ramfs_traverse(path);
    if (parent == NULL)
    {
        return NULL;
    }

    const char* filename = vfs_basename(path);
    if (filename == NULL)
    {
        return NULL;
    }

    return ram_dir_find_file(parent, filename);
}

/*static RamFile* ramfs_find_dir(const char* path)
{
    RamDir* parent = ramfs_traverse(path);
    if (parent == NULL)
    {
        return NULL;
    }

    const char* dirname = vfs_basename(path);
    if (dirname == NULL)
    {
        return NULL;
    }

    return ram_dir_find_dir(parent, dirname);
}*/

File* ramfs_open(Drive* drive, const char* path)
{
    RamFile* ramFile = ramfs_find_file(path);
    if (ramFile == NULL)
    {
        return NULLPTR(EPATH);
    }

    return file_new(drive, ramFile);
}

uint64_t ramfs_read(File* file, void* buffer, uint64_t count)
{
    RamFile const* ramFile = file->internal;

    uint64_t pos = file->position;
    uint64_t readCount = pos <= ramFile->size ? MIN(count, ramFile->size - pos) : 0;
    file->position += readCount;

    memcpy(buffer, ramFile->data + pos, readCount);

    return readCount;
}

uint64_t ramfs_seek(File* file, int64_t offset, uint8_t origin)
{
    RamFile const* ramFile = file->internal;

    uint64_t position;
    switch (origin)
    {
    case SEEK_SET:
    {
        position = offset;
    }
    break;
    case SEEK_CUR:
    {
        position = file->position + offset;
    }
    break;
    case SEEK_END:
    {
        position = ramFile->size - offset;
    }
    break;
    default:
    {
        position = 0;
    }
    break;
    }

    file->position = MIN(position, ramFile->size);
    return position;
}

void ramfs_init(RamDir* ramRoot)
{
    tty_start_message("Ramfs initializing");

    root = ramRoot;

    memset(&ramfs, 0, sizeof(Filesystem));
    ramfs.name = "ramfs";
    ramfs.open = ramfs_open;
    ramfs.read = ramfs_read;
    ramfs.seek = ramfs_seek;

    if (vfs_mount('B', &ramfs, NULL) == ERR)
    {
        tty_print("Failed to mount ramfs");
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}