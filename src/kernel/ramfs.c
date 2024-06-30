#include "ramfs.h"

#include "log.h"
#include "sched.h"
#include "vfs.h"

#include <bootloader/boot_info.h>

#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>

static fs_t ramfs;
static ram_dir_t* root;

static ram_file_t* ram_dir_find_file(ram_dir_t* dir, const char* filename)
{
    ram_file_t* file;
    LIST_FOR_EACH(file, &dir->files)
    {
        if (name_compare(file->name, filename))
        {
            return file;
        }
    }

    return NULL;
}

static ram_dir_t* ram_dir_find_dir(ram_dir_t* dir, const char* dirname)
{
    ram_dir_t* child;
    LIST_FOR_EACH(child, &dir->children)
    {
        if (name_compare(child->name, dirname))
        {
            return child;
        }
    }

    return NULL;
}

static ram_dir_t* ramfs_traverse(const char* path)
{
    ram_dir_t* dir = root;
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

static ram_file_t* ramfs_find_file(const char* path)
{
    ram_dir_t* parent = ramfs_traverse(path);
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

static ram_dir_t* ramfs_find_dir(const char* path)
{
    ram_dir_t* parent = ramfs_traverse(path);
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
}

static uint64_t ramfs_read(file_t* file, void* buffer, uint64_t count)
{
    ram_file_t* internal = file->internal;

    uint64_t pos = file->position;
    uint64_t readCount = (pos <= internal->size) ? MIN(count, internal->size - pos) : 0;

    file->position += readCount;
    memcpy(buffer, internal->data + pos, readCount);

    return readCount;
}

static uint64_t ramfs_seek(file_t* file, int64_t offset, uint8_t origin)
{
    ram_file_t* internal = file->internal;

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

static uint64_t ramfs_open(volume_t* volume, file_t* file, const char* path)
{
    ram_file_t* ramFile = ramfs_find_file(path);
    if (ramFile == NULL)
    {
        return ERROR(EPATH);
    }

    file->ops.read = ramfs_read;
    file->ops.seek = ramfs_seek;
    file->internal = ramFile;

    return 0;
}

static uint64_t ramfs_stat(volume_t* volume, const char* path, stat_t* buffer)
{
    buffer->size = 0;

    ram_dir_t* parent = ramfs_traverse(path);
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

static uint64_t ramfs_mount(volume_t* volume)
{
    volume->open = ramfs_open;
    volume->stat = ramfs_stat;

    return 0;
}

static ram_dir_t* ramfs_load_dir(ram_dir_t* in)
{
    ram_dir_t* out = malloc(sizeof(ram_dir_t));
    list_entry_init(&out->base);
    strcpy(out->name, in->name);
    list_init(&out->children);
    list_init(&out->files);

    ram_dir_t* child;
    LIST_FOR_EACH(child, &in->children)
    {
        list_push(&out->children, ramfs_load_dir(child));
    }

    ram_file_t* inFile;
    LIST_FOR_EACH(inFile, &in->files)
    {
        ram_file_t* outFile = malloc(sizeof(ram_file_t));
        list_entry_init(&outFile->base);
        strcpy(outFile->name, inFile->name);
        outFile->size = inFile->size;
        outFile->data = malloc(outFile->size);
        memcpy(outFile->data, inFile->data, outFile->size);

        list_push(&out->files, outFile);
    }

    return out;
}

void ramfs_init(ram_dir_t* ramRoot)
{
    root = ramfs_load_dir(ramRoot);

    memset(&ramfs, 0, sizeof(fs_t));
    ramfs.name = "ramfs";
    ramfs.mount = ramfs_mount;

    LOG_ASSERT(vfs_mount("home", &ramfs) != ERR, "mount fail");

    log_print("ramfs: initialized");
}
