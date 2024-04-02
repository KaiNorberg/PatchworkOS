#include "ramfs.h"

#include <string.h>

#include "vfs/vfs.h"
#include "vfs/utils/utils.h"
#include "tty/tty.h"
#include "sched/sched.h"
#include "utils/utils.h"

static Filesystem ramfs;
static RamDir* root;

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

static inline RamDir* ramfs_traverse(const char* path)
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

uint64_t ramfs_open(Disk* disk, File* file, const char* path)
{
    RamDir* ramDir = ramfs_traverse(path);
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

uint64_t ramfs_read(File* file, void* buffer, uint64_t count)
{
    RamFile const* ramFile = file->context;

    size_t pos = file->position;
    size_t readCount = pos <= ramFile->size ? MIN(count, ramFile->size - pos) : 0;
    atomic_fetch_add(&file->position, readCount);

    memcpy(buffer, ramFile->data + pos, readCount);
    return readCount;
}

uint64_t ramfs_seek(File* file, int64_t offset, uint8_t origin)
{
    RamFile const* ramFile = file->context;

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
        position = atomic_load(&file->position) + offset;
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

    atomic_store(&file->position, position);

    return position;
}

void ramfs_init(RamDir* ramRoot)
{
    tty_start_message("Ramfs initializing");

    root = ramRoot;

    memset(&ramfs, 0, sizeof(Filesystem));
    strcpy(ramfs.name, "ramfs");
    ramfs.diskFuncs.open = ramfs_open;
    ramfs.fileFuncs.read = ramfs_read;
    ramfs.fileFuncs.seek = ramfs_seek;

    if (vfs_mount("ram", NULL, &ramfs) == ERR)
    {
        tty_print("Failed to mount ramfs");
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}