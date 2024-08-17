#include "ramfs.h"

#include "log.h"
#include "sched.h"
#include "sysfs.h"
#include "vfs.h"

#include <bootloader/boot_info.h>

#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>

static node_t* root;

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

static file_ops_t fileOps = {
    .read = ramfs_read,
    .seek = ramfs_seek,
};

static file_t* ramfs_open(volume_t* volume, const char* path)
{
    node_t* node = node_traverse(root, path, VFS_NAME_SEPARATOR);
    if (node == NULL)
    {
        return NULLPTR(EPATH);
    }
    else if (node->type == RAMFS_DIR)
    {
        return NULLPTR(EISDIR);
    }
    ram_file_t* ramFile = (ram_file_t*)node;

    file_t* file = file_new(volume);
    file->ops = &fileOps;
    file->private = ramFile;

    return file;
}

static uint64_t ramfs_stat(volume_t* volume, const char* path, stat_t* stat)
{
    node_t* node = node_traverse(root, path, VFS_NAME_SEPARATOR);
    if (node == NULL)
    {
        return ERROR(EPATH);
    }

    stat->size = 0;
    stat->type = node->type == RAMFS_FILE ? STAT_FILE : STAT_DIR;

    return 0;
}

static uint64_t ramfs_listdir(volume_t* volume, const char* path, dir_entry_t* entries, uint64_t amount)
{
    node_t* node = node_traverse(root, path, VFS_NAME_SEPARATOR);
    if (node == NULL)
    {
        return ERROR(EPATH);
    }
    else if (node->type == SYSFS_RESOURCE)
    {
        return ERROR(ENOTDIR);
    }

    uint64_t index = 0;
    uint64_t total = 0;

    node_t* child;
    LIST_FOR_EACH(child, &node->children)
    {
        dir_entry_t entry = {0};
        strcpy(entry.name, child->name);
        entry.type = child->type == SYSFS_RESOURCE ? STAT_RES : STAT_DIR;

        dir_entry_push(entries, amount, &index, &total, &entry);
    }

    return total;
}

static volume_ops_t volumeOps = {
    .open = ramfs_open,
    .stat = ramfs_stat,
    .listdir = ramfs_listdir,
};

static uint64_t ramfs_mount(const char* label)
{
    return vfs_attach_simple(label, &volumeOps);
}

static fs_t ramfs = {
    .name = "ramfs",
    .mount = ramfs_mount,
};

static node_t* ramfs_load_dir(node_t* in)
{
    node_t* node = malloc(sizeof(node_t));
    node_init(node, in->name, RAMFS_DIR);

    node_t* inChild;
    LIST_FOR_EACH(inChild, &in->children)
    {
        if (inChild->type == RAMFS_DIR)
        {
            node_t* inDir = (node_t*)inChild;

            node_t* outDir = ramfs_load_dir(inDir);
            node_push(node, outDir);
        }
        else if (inChild->type == RAMFS_FILE)
        {
            ram_file_t* inFile = (ram_file_t*)inChild;

            ram_file_t* outFile = malloc(sizeof(ram_file_t));
            node_init(&outFile->node, inFile->node.name, RAMFS_FILE);
            outFile->size = inFile->size;
            outFile->data = malloc(outFile->size);
            memcpy(outFile->data, inFile->data, outFile->size);

            node_push(node, &outFile->node);
        }
    }

    return node;
}

void ramfs_init(ram_disk_t* disk)
{
    root = ramfs_load_dir(disk->root);
    LOG_ASSERT(vfs_mount("home", &ramfs) != ERR, "mount fail");

    log_print("ramfs: initialized");
}
