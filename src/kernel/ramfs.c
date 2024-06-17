#include "ramfs.h"

#include "debug.h"
#include "sched.h"
#include "vfs.h"

#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

static Filesystem ramfs;
static RamDir* root;

static RamFile* ram_dir_find_file(RamDir* dir, const char* filename)
{
    RamFile* file = dir->firstFile;
    while (file != NULL)
    {
        if (name_compare(file->name, filename))
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
        if (name_compare(child->name, dirname))
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
    const char* dirname = dir_name_first(path);
    while (dirname != NULL)
    {
        dir = ram_dir_find_dir(dir, dirname);
        if (dir == NULL)
        {
            return NULL;
        }

        dirname = dir_name_next(dirname);
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

static uint64_t ramfs_read(File* file, void* buffer, uint64_t count)
{
    RamFile* internal = file->internal;

    uint64_t pos = file->position;
    uint64_t readCount = (pos <= internal->size) ? MIN(count, internal->size - pos) : 0;

    file->position += readCount;
    memcpy(buffer, internal->data + pos, readCount);

    return readCount;
}

static uint64_t ramfs_seek(File* file, int64_t offset, uint8_t origin)
{
    RamFile* internal = file->internal;

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
        position = internal->size - offset;
    }
    break;
    default:
    {
        position = 0;
    }
    break;
    }

    file->position = MIN(position, internal->size);
    return position;
}

static uint64_t ramfs_open(Volume* volume, File* file, const char* path)
{
    RamFile* ramFile = ramfs_find_file(path);
    if (ramFile == NULL)
    {
        return ERROR(EPATH);
    }

    file->methods.read = ramfs_read;
    file->methods.seek = ramfs_seek;
    file->internal = ramFile;

    return 0;
}

static uint64_t ramfs_stat(Volume* volume, const char* path, stat_t* buffer)
{
    buffer->size = 0;

    RamDir* parent = ramfs_traverse(path);
    if (parent == NULL)
    {
        return ERR;
    }

    const char* name = vfs_basename(path);
    if (ram_dir_find_file(parent, name) != NULL)
    {
        buffer->type = STAT_FILE;
    }
    else if (ram_dir_find_dir(parent, name) != NULL)
    {
        buffer->type = STAT_DIR;
    }
    else
    {
        return ERROR(EPATH);
    }

    return 0;
}

static uint64_t ramfs_mount(Volume* volume)
{
    volume->open = ramfs_open;
    volume->stat = ramfs_stat;

    return 0;
}

void ramfs_init(RamDir* ramRoot)
{
    root = ramRoot;

    memset(&ramfs, 0, sizeof(Filesystem));
    ramfs.name = "ramfs";
    ramfs.mount = ramfs_mount;

    DEBUG_ASSERT(vfs_mount("home", &ramfs) != ERR, "mount fail");
}
