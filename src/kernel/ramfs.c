#include "ramfs.h"

#include "log.h"
#include "sched.h"
#include "vfs.h"

#include <bootloader/boot_info.h>

#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>

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
    const char* dirname = name_first(path);
    while (dirname != NULL)
    {
        dir = ram_dir_find_dir(dir, dirname);
        if (dir == NULL)
        {
            return NULL;
        }

        dirname = name_next(dirname);
    }

    return dir;
}

static ram_dir_t* ramfs_traverse_parent(const char* path)
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
    ram_dir_t* parent = ramfs_traverse_parent(path);
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
    ram_dir_t* parent = ramfs_traverse_parent(path);
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

static uint64_t ramfs_open(file_t* file, const char* path)
{
    ram_file_t* ramFile = ramfs_find_file(path);
    if (ramFile == NULL)
    {
        return ERROR(EPATH);
    }

    file->private = ramFile;
    return 0;
}

static uint64_t ramfs_read(file_t* file, void* buffer, uint64_t count)
{
    ram_file_t* private = file->private;

    count = (file->pos <= private->size) ? MIN(count, private->size - file->pos) : 0;
    memcpy(buffer, private->data + file->pos, count);
    file->pos += count;

    return count;
}

static uint64_t ramfs_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    ram_file_t* private = file->private;

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
        position = file->pos + offset;
    }
    break;
    case SEEK_END:
    {
        position = private->size - offset;
    }
    break;
    default:
    {
        position = 0;
    }
    break;
    }

    file->pos = MIN(position, private->size);
    return position;
}

static uint64_t ramfs_stat(volume_t* volume, const char* path, stat_t* buffer)
{
    buffer->size = 0;

    ram_dir_t* parent = ramfs_traverse_parent(path);
    if (parent == NULL)
    {
        return ERROR(EPATH);
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

static uint64_t ramfs_listdir(volume_t* volume, const char* path, dir_entry_t* entries, uint64_t amount)
{
    ram_dir_t* parent = ramfs_traverse(path);
    if (parent == NULL)
    {
        return ERROR(EPATH);
    }

    uint64_t index = 0;
    uint64_t total = 0;

    ram_dir_t* dir;
    LIST_FOR_EACH(dir, &parent->children)
    {
        dir_entry_t entry = {0};
        strcpy(entry.name, dir->name);
        entry.type = STAT_DIR;

        dir_entry_push(entries, amount, &index, &total, &entry);
    }

    ram_file_t* file;
    LIST_FOR_EACH(file, &parent->files)
    {
        dir_entry_t entry = {0};
        strcpy(entry.name, file->name);
        entry.type = STAT_FILE;

        dir_entry_push(entries, amount, &index, &total, &entry);
    }

    return total;
}

static volume_ops_t volumeOps = {
    .stat = ramfs_stat,
    .listdir = ramfs_listdir,
};

static file_ops_t fileOps = {
    .open = ramfs_open,
    .read = ramfs_read,
    .seek = ramfs_seek,
};

static uint64_t ramfs_mount(const char* label)
{
    return vfs_attach_simple(label, &volumeOps, &fileOps);
}

static fs_t ramfs = {
    .name = "ramfs",
    .mount = ramfs_mount,
};

static ram_dir_t* ramfs_load_dir(ram_dir_t* in)
{
    ram_dir_t* out = malloc(sizeof(ram_dir_t));
    list_entry_init(&out->entry);
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
        list_entry_init(&outFile->entry);
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

    LOG_ASSERT(vfs_mount("home", &ramfs) != ERR, "mount fail");

    log_print("ramfs: initialized");
}
