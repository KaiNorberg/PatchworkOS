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

static sysdir_t root;
static lock_t lock;

static sysdir_t* sysdir_ref(sysdir_t* dir)
{
    atomic_fetch_add(&dir->ref, 1);
    return dir;
}

static void sysdir_deref(sysdir_t* dir)
{
    if (atomic_fetch_sub(&dir->ref, 1) <= 1)
    {
        if (dir->onFree != NULL)
        {
            dir->onFree(dir);
        }

        free(dir);
    }
}

static void resource_deref(resource_t* resource)
{
    if (atomic_fetch_sub(&resource->ref, 1) <= 1)
    {
        if (resource->ops->onFree != NULL)
        {
            resource->ops->onFree(resource);
        }

        sysdir_deref(resource->dir);

        free(resource);
    }
}

static file_t* sysfs_open(volume_t* volume, const char* path)
{
    LOCK_DEFER(&lock);

    node_t* node = node_traverse(&root.node, path, VFS_NAME_SEPARATOR);
    if (node == NULL)
    {
        return ERRPTR(EPATH);
    }
    else if (node->type != SYSFS_RESOURCE)
    {
        return ERRPTR(EISDIR);
    }
    resource_t* resource = NODE_CONTAINER(node, resource_t, node);

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

    node_t* node = node_traverse(&root.node, path, VFS_NAME_SEPARATOR);
    if (node == NULL)
    {
        return ERROR(EPATH);
    }
    else if (node->type != SYSFS_RESOURCE)
    {
        return ERROR(EISDIR);
    }
    resource_t* resource = NODE_CONTAINER(node, resource_t, node);

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

    node_t* node = node_traverse(&root.node, path, VFS_NAME_SEPARATOR);
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

    node_t* node = node_traverse(&root.node, path, VFS_NAME_SEPARATOR);
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

static void sysfs_cleanup(volume_t* volume, file_t* file)
{
    if (file->resource->ops->onCleanup != NULL)
    {
        file->resource->ops->onCleanup(file->resource, file);
    }

    resource_deref(file->resource);
}

static volume_ops_t volumeOps = {
    .open = sysfs_open,
    .open2 = sysfs_open2,
    .stat = sysfs_stat,
    .listdir = sysfs_listdir,
    .cleanup = sysfs_cleanup,
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
    node_init(&root.node, "root", SYSFS_DIR);
    root.private = NULL;
    root.onFree = NULL;
    atomic_init(&root.ref, 1);

    lock_init(&lock);

    ASSERT_PANIC(vfs_mount("sys", &sysfs) != ERR, "mount fail");

    printf("sysfs: init");
}

static node_t* sysfs_traverse_and_allocate(const char* path)
{
    node_t* parent = &root.node;
    const char* name = name_first(path);
    while (name != NULL)
    {
        node_t* child = node_find(parent, name, VFS_NAME_SEPARATOR);
        if (child == NULL)
        {
            sysdir_t* dir = malloc(sizeof(sysdir_t));
            if (dir == NULL)
            {
                return NULL;
            }

            char nameCopy[MAX_NAME];
            name_copy(nameCopy, name);
            node_init(&dir->node, nameCopy, SYSFS_DIR);
            dir->private = NULL;
            dir->onFree = NULL;
            atomic_init(&dir->ref, 1);
            node_push(parent, &dir->node);

            child = &dir->node;
        }
        if (child->type != SYSFS_DIR)
        {
            return ERRPTR(ENOTDIR);
        }

        parent = child;
        name = name_next(name);
    }

    return parent;
}

sysdir_t* sysfs_mkdir(const char* path, const char* dirname, sysdir_on_free_t onFree, void* private)
{
    LOCK_DEFER(&lock);

    node_t* parent = sysfs_traverse_and_allocate(path);
    if (parent == NULL)
    {
        return NULL;
    }

    sysdir_t* dir = malloc(sizeof(sysdir_t));
    if (dir == NULL)
    {
        return NULL;
    }
    node_init(&dir->node, dirname, SYSFS_DIR);
    dir->private = private;
    dir->onFree = onFree;
    atomic_init(&dir->ref, 1);
    node_push(parent, &dir->node);

    return dir;
}

uint64_t sysfs_create(sysdir_t* dir, const char* filename, const resource_ops_t* ops, void* private)
{
    LOCK_DEFER(&lock);

    if (dir == NULL)
    {
        dir = &root;
    }

    resource_t* resource = malloc(sizeof(resource_t));
    if (resource == NULL)
    {
        return ERR;
    }
    node_init(&resource->node, filename, SYSFS_RESOURCE);
    resource->private = private;
    resource->ops = ops;
    atomic_init(&resource->ref, 1);
    atomic_init(&resource->hidden, false);
    resource->dir = sysdir_ref(dir);

    node_push(&dir->node, &resource->node);
    return 0;
}

void sysfs_rmdir(sysdir_t* dir)
{
    LOCK_DEFER(&lock);

    node_t* node;
    node_t* temp;
    LIST_FOR_EACH_SAFE(node, temp, &dir->node.children, entry)
    {
        ASSERT_PANIC(node->type == SYSFS_RESOURCE, "attempt to remove directory containing directories");
        resource_t* resource = NODE_CONTAINER(node, resource_t, node);

        atomic_store(&resource->hidden, true);
        node_remove(&resource->node);
        resource_deref(resource);
    }

    node_remove(&dir->node);
    sysdir_deref(dir);
}

resource_t* sysfs_expose(const char* path, const char* filename, const resource_ops_t* ops, void* private)
{
    LOCK_DEFER(&lock);

    node_t* parent = sysfs_traverse_and_allocate(path);
    if (parent == NULL)
    {
        return NULL;
    }

    resource_t* resource = malloc(sizeof(resource_t));
    if (resource == NULL)
    {
        return NULL;
    }
    node_init(&resource->node, filename, SYSFS_RESOURCE);
    resource->private = private;
    resource->ops = ops;
    atomic_init(&resource->ref, 2);
    atomic_init(&resource->hidden, false);
    resource->dir = sysdir_ref(NODE_CONTAINER(parent, sysdir_t, node));

    node_push(parent, &resource->node);
    return resource;
}

void sysfs_hide(resource_t* resource)
{
    LOCK_DEFER(&lock);

    atomic_store(&resource->hidden, true);
    node_remove(&resource->node);
    resource_deref(resource);
}