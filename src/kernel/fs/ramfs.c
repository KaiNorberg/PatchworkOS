#include "ramfs.h"

#include "log/log.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "sched/thread.h"
#include "sysfs.h"
#include "vfs.h"
#include "view.h"

#include <boot/boot_info.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>

static ram_dir_t* root;
static lock_t lock;

static uint64_t ramfs_readdir(file_t* file, stat_t* infos, uint64_t amount)
{
    LOCK_DEFER(&lock);
    ram_dir_t* ramDir = CONTAINER_OF(file->private, ram_dir_t, node);

    uint64_t index = 0;
    uint64_t total = 0;

    node_t* child;
    LIST_FOR_EACH(child, &ramDir->node.children, entry)
    {
        stat_t info = {0};
        strcpy(info.name, child->name);
        info.type = child->type == RAMFS_FILE ? STAT_FILE : STAT_DIR;
        info.size = 0;

        readdir_push(infos, amount, &index, &total, &info);
    }

    return total;
}

static file_ops_t dirOps = {
    .readdir = ramfs_readdir,
};

static uint64_t ramfs_read(file_t* file, void* buffer, uint64_t count)
{
    LOCK_DEFER(&lock);
    ram_file_t* ramFile = CONTAINER_OF(file->private, ram_file_t, node);

    if (ramFile->data == NULL)
    {
        return 0;
    }

    return BUFFER_READ(file, buffer, count, ramFile->data, ramFile->size);
}

static uint64_t ramfs_write(file_t* file, const void* buffer, uint64_t count)
{
    LOCK_DEFER(&lock);
    ram_file_t* ramFile = CONTAINER_OF(file->private, ram_file_t, node);

    if (file->flags & PATH_APPEND)
    {
        file->pos = ramFile->size;
    }

    if (file->pos + count >= ramFile->size)
    {
        void* newData = heap_realloc(ramFile->data, file->pos + count, HEAP_VMM);
        if (newData == NULL)
        {
            return ERR;
        }
        memset(newData + ramFile->size, 0, file->pos + count - ramFile->size);
        ramFile->data = newData;
        ramFile->size = file->pos + count;
    }

    memcpy(ramFile->data + file->pos, buffer, count);
    file->pos += count;
    return 0;
}

static uint64_t ramfs_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    LOCK_DEFER(&lock);
    ram_file_t* ramFile = CONTAINER_OF(file->private, ram_file_t, node);

    return BUFFER_SEEK(file, offset, origin, ramFile->size);
}

static file_ops_t fileOps = {
    .read = ramfs_read,
    .write = ramfs_write,
    .seek = ramfs_seek,
};

static file_t* ramfs_open(volume_t* volume, const path_t* path)
{
    LOCK_DEFER(&lock);

    node_t* node = path_traverse_node(path, &root->node);
    if (node == NULL)
    {
        if (!(path->flags & PATH_CREATE))
        {
            return ERRPTR(ENOENT);
        }

        const char* filename = path_last_name(path);
        if (filename == NULL)
        {
            return ERRPTR(EINVAL);
        }
        node_t* parent = path_traverse_node_parent(path, &root->node);
        if (parent == NULL)
        {
            return ERRPTR(ENOENT);
        }

        if (path->flags & PATH_DIRECTORY)
        {
            ram_dir_t* ramDir = heap_alloc(sizeof(ram_dir_t), HEAP_NONE);
            if (ramDir == NULL)
            {
                return NULL;
            }
            node_init(&ramDir->node, filename, RAMFS_DIR);
            ramDir->openedAmount = 0;
            node_push(parent, &ramDir->node);

            node = &ramDir->node;
        }
        else
        {
            ram_file_t* ramFile = heap_alloc(sizeof(ram_file_t), HEAP_NONE);
            if (ramFile == NULL)
            {
                return NULL;
            }
            node_init(&ramFile->node, filename, RAMFS_FILE);
            ramFile->size = 0;
            ramFile->data = NULL;
            ramFile->openedAmount = 0;
            node_push(parent, &ramFile->node);

            node = &ramFile->node;
        }
    }
    else if ((path->flags & PATH_CREATE) && (path->flags & PATH_EXCLUSIVE))
    {
        return ERRPTR(EEXIST);
    }
    else if ((path->flags & PATH_DIRECTORY) && node->type != RAMFS_DIR)
    {
        return ERRPTR(ENOTDIR);
    }
    else if (!(path->flags & PATH_DIRECTORY) && node->type != RAMFS_FILE)
    {
        return ERRPTR(EISDIR);
    }

    file_t* file = file_new(volume, path, PATH_CREATE | PATH_EXCLUSIVE | PATH_APPEND | PATH_TRUNCATE | PATH_DIRECTORY);
    if (file == NULL)
    {
        return NULL;
    }
    file->private = node;

    if (node->type == RAMFS_FILE)
    {
        ram_file_t* ramFile = CONTAINER_OF(node, ram_file_t, node);
        ramFile->openedAmount++;

        file->ops = &fileOps;
        if (path->flags & PATH_TRUNCATE)
        {
            heap_free(ramFile->data);
            ramFile->data = NULL;
            ramFile->size = 0;
        }
    }
    else
    {
        ram_dir_t* ramDir = CONTAINER_OF(node, ram_dir_t, node);
        ramDir->openedAmount++;
        file->ops = &dirOps;
    }

    return file;
}

static void ramfs_cleanup(volume_t* volume, file_t* file)
{
    LOCK_DEFER(&lock);

    node_t* node = file->private;

    if (node->type == RAMFS_FILE)
    {
        ram_file_t* ramFile = CONTAINER_OF(node, ram_file_t, node);
        ramFile->openedAmount--;
    }
    else
    {
        ram_dir_t* ramDir = CONTAINER_OF(node, ram_dir_t, node);
        ramDir->openedAmount--;
    }
}

static uint64_t ramfs_stat(volume_t* volume, const path_t* path, stat_t* stat)
{
    LOCK_DEFER(&lock);

    node_t* node = path_traverse_node(path, &root->node);
    if (node == NULL)
    {
        return ERROR(ENOENT);
    }

    strcpy(stat->name, node->name);
    stat->type = node->type == RAMFS_FILE ? STAT_FILE : STAT_DIR;
    stat->size = 0;

    return 0;
}

static uint64_t ramfs_rename(volume_t* volume, const path_t* oldpath, const path_t* newpath)
{
    LOCK_DEFER(&lock);

    node_t* node = path_traverse_node(oldpath, &root->node);
    if (node == NULL || node == &root->node)
    {
        return ERROR(ENOENT);
    }

    node_t* dest = path_traverse_node_parent(newpath, &root->node);
    if (dest == NULL)
    {
        return ERROR(ENOENT);
    }
    else if (dest->type != RAMFS_DIR)
    {
        return ERROR(ENOTDIR);
    }

    const char* newName = path_last_name(newpath);
    if (newName == NULL)
    {
        return ERROR(EINVAL);
    }
    strcpy(node->name, newName);
    node_remove(node);
    node_push(dest, node);
    return 0;
}

static uint64_t ramfs_remove(volume_t* volume, const path_t* path)
{
    LOCK_DEFER(&lock);

    node_t* node = path_traverse_node(path, &root->node);
    if (node == NULL)
    {
        return ERROR(ENOENT);
    }

    if (node->type == RAMFS_FILE)
    {
        ram_file_t* ramFile = CONTAINER_OF(node, ram_file_t, node);
        if (ramFile->openedAmount != 0)
        {
            return ERROR(EBUSY);
        }
        node_remove(&ramFile->node);
        heap_free(ramFile->data);
        heap_free(ramFile);
    }
    else
    {
        ram_dir_t* ramDir = CONTAINER_OF(node, ram_dir_t, node);
        if (ramDir->openedAmount != 0 || ramDir->node.childAmount != 0)
        {
            return ERROR(EBUSY);
        }
        node_remove(&ramDir->node);
        heap_free(ramDir);
    }

    return 0;
}

static volume_ops_t volumeOps = {
    .open = ramfs_open,
    .cleanup = ramfs_cleanup,
    .stat = ramfs_stat,
    .rename = ramfs_rename,
    .remove = ramfs_remove,
};

static uint64_t ramfs_mount(const char* label)
{
    return vfs_attach_simple(label, &volumeOps);
}

static fs_t ramfs = {
    .name = "ramfs",
    .mount = ramfs_mount,
};

static ram_dir_t* ramfs_load_dir(ram_dir_t* in)
{
    ram_dir_t* newDir = heap_alloc(sizeof(ram_dir_t), HEAP_NONE);
    assert(newDir != NULL);
    node_init(&newDir->node, in->node.name, RAMFS_DIR);
    newDir->openedAmount = 0;

    node_t* child;
    LIST_FOR_EACH(child, &in->node.children, entry)
    {
        if (child->type == RAMFS_DIR)
        {
            ram_dir_t* dir = CONTAINER_OF(child, ram_dir_t, node);

            node_push(&newDir->node, &ramfs_load_dir(dir)->node);
        }
        else if (child->type == RAMFS_FILE)
        {
            ram_file_t* file = CONTAINER_OF(child, ram_file_t, node);

            ram_file_t* newFile = heap_alloc(sizeof(ram_file_t), HEAP_NONE);
            assert(newFile != NULL);
            node_init(&newFile->node, file->node.name, RAMFS_FILE);
            newFile->size = file->size;
            newFile->data = heap_alloc(newFile->size, HEAP_VMM);
            memcpy(newFile->data, file->data, newFile->size);
            newFile->openedAmount = 0;

            node_push(&newDir->node, &newFile->node);
        }
    }

    return newDir;
}

void ramfs_init(ram_disk_t* disk)
{
    root = ramfs_load_dir(disk->root);
    assert(vfs_mount("home", &ramfs) != ERR);
    lock_init(&lock);

    log_print(LOG_INFO, "ramfs: init\n");
}
