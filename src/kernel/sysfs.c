#include "sysfs.h"

#include "lock.h"
#include "log.h"
#include "sched.h"
#include "vfs.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

static node_t root;
static lock_t lock;

static file_t* sysfs_open(volume_t* volume, const char* path)
{
    LOCK_DEFER(&lock);

    node_t* node = node_traverse(&root, path, VFS_NAME_SEPARATOR);
    if (node == NULL)
    {
        return ERRPTR(EPATH);
    }
    else if (node->type == SYSFS_SYSTEM)
    {
        return ERRPTR(EISDIR);
    }
    resource_t* resource = (resource_t*)node;

    if (resource->ops->open == NULL)
    {
        return ERRPTR(EACCES);
    }

    file_t* file = resource->ops->open(volume, resource);
    if (file == NULL)
    {
        return NULL;
    }

    file->resource = resource;
    atomic_fetch_add(&resource->ref, 1);
    return file;
}

static uint64_t sysfs_open2(volume_t* volume, const char* path, file_t* files[2])
{
    LOCK_DEFER(&lock);

    node_t* node = node_traverse(&root, path, VFS_NAME_SEPARATOR);
    if (node == NULL)
    {
        return ERROR(EPATH);
    }
    else if (node->type == SYSFS_SYSTEM)
    {
        return ERROR(EISDIR);
    }
    resource_t* resource = (resource_t*)node;

    if (resource->ops->open2 == NULL)
    {
        return ERROR(EACCES);
    }

    if (resource->ops->open2(volume, resource, files) == ERR)
    {
        return ERR;
    }

    files[0]->resource = resource;
    files[1]->resource = resource;
    atomic_fetch_add(&resource->ref, 2);
    return 0;
}

static uint64_t sysfs_stat(volume_t* volume, const char* path, stat_t* stat)
{
    LOCK_DEFER(&lock);

    node_t* node = node_traverse(&root, path, VFS_NAME_SEPARATOR);
    if (node == NULL)
    {
        return ERROR(EPATH);
    }

    stat->size = 0;
    stat->type = node->type == SYSFS_RESOURCE ? STAT_FILE : STAT_DIR;

    return 0;
}

static uint64_t sysfs_listdir(volume_t* volume, const char* path, dir_entry_t* entries, uint64_t amount)
{
    LOCK_DEFER(&lock);

    node_t* node = node_traverse(&root, path, VFS_NAME_SEPARATOR);
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
    LIST_FOR_EACH(child, &node->children, entry)
    {
        dir_entry_t entry = {0};
        strcpy(entry.name, child->name);
        entry.type = child->type == SYSFS_RESOURCE ? STAT_FILE : STAT_DIR;

        dir_entry_push(entries, amount, &index, &total, &entry);
    }

    return total;
}

static volume_ops_t volumeOps = {
    .open = sysfs_open,
    .open2 = sysfs_open2,
    .stat = sysfs_stat,
    .listdir = sysfs_listdir,
};

static uint64_t sysfs_mount(const char* label)
{
    return vfs_attach_simple(label, &volumeOps);
}

static fs_t sysfs = {
    .name = "sysfs",
    .mount = sysfs_mount,
};

void sysfs_init(void)
{
    node_init(&root, "root", SYSFS_SYSTEM);
    lock_init(&lock);

    ASSERT_PANIC(vfs_mount("sys", &sysfs) != ERR, "mount fail");

    printf("sysfs: init");
}

resource_t* sysfs_expose(const char* path, const char* filename, const resource_ops_t* ops, void* private)
{
    LOCK_DEFER(&lock);

    node_t* parent = &root;
    const char* name = name_first(path);
    while (name != NULL)
    {
        node_t* child = node_find(parent, name, VFS_NAME_SEPARATOR);
        if (child == NULL)
        {
            child = malloc(sizeof(node_t));
            char nameCopy[MAX_NAME];
            name_copy(nameCopy, name);
            node_init(child, nameCopy, SYSFS_SYSTEM);
            node_push(parent, child);
        }

        parent = child;
        name = name_next(name);
    }

    resource_t* resource = malloc(sizeof(resource_t));
    node_init(&resource->node, filename, SYSFS_RESOURCE);
    resource->ops = ops;
    resource->private = private;
    atomic_init(&resource->ref, 1);
    atomic_init(&resource->hidden, false);
    node_push(parent, &resource->node);

    return resource;
}

void sysfs_hide(resource_t* resource)
{
    lock_acquire(&lock);
    node_remove(&resource->node);
    lock_release(&lock);

    atomic_store(&resource->hidden, true);
    if (atomic_fetch_sub(&resource->ref, 1) <= 1)
    {
        if (resource->ops->onFree != NULL)
        {
            resource->ops->onFree(resource);
        }

        free(resource);
    }
}
