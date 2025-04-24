#include "sysfs.h"

#include "rwlock.h"
#include "log.h"
#include "path.h"
#include "sched.h"
#include "sys/node.h"
#include "vfs.h"

#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

static sysdir_t root;
static rwlock_t lock;

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

static resource_t* resource_ref(resource_t* resource)
{
    atomic_fetch_add(&resource->ref, 1);
    return resource;
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

static resource_t* sysfs_resource_get(const path_t* path)
{
    RWLOCK_READ_DEFER(&lock);
    node_t* node = path_traverse_node(path, &root.node);
    if (node == NULL)
    {
        return ERRPTR(EPATH);
    }
    else if (node->type != SYSFS_RESOURCE)
    {
        return ERRPTR(EISDIR);
    }
    return resource_ref(NODE_CONTAINER(node, resource_t, node));
}

static file_t* sysfs_open(volume_t* volume, const path_t* path)
{
    resource_t* resource = sysfs_resource_get(path);
    if (resource == NULL)
    {
        return NULL;
    }

    if (resource->ops->open == NULL)
    {
        resource_deref(resource);
        return ERRPTR(ENOOP);
    }

    file_t* file = resource->ops->open(volume, resource);
    if (file == NULL)
    {
        resource_deref(resource);
        return NULL;
    }

    file->resource = resource;
    return file;
}

static uint64_t sysfs_open2(volume_t* volume, const path_t* path, file_t* files[2])
{
    resource_t* resource = sysfs_resource_get(path); // First ref
    if (resource == NULL)
    {
        return ERR;
    }

    if (resource->ops->open2 == NULL)
    {
        resource_deref(resource);
        return ERROR(ENOOP);
    }

    if (resource->ops->open2(volume, resource, files) == ERR)
    {
        resource_deref(resource);
        return ERR;
    }

    files[0]->resource = resource; // First ref
    files[1]->resource = resource_ref(resource);
    return 0;
}

static uint64_t sysfs_stat(volume_t* volume, const path_t* path, stat_t* stat)
{
    RWLOCK_READ_DEFER(&lock);

    node_t* node = path_traverse_node(path, &root.node);
    if (node == NULL)
    {
        return ERROR(EPATH);
    }

    stat->size = 0;
    stat->type = node->type == SYSFS_RESOURCE ? STAT_FILE : STAT_DIR;

    return 0;
}

static uint64_t sysfs_listdir(volume_t* volume, const path_t* path, dir_entry_t* entries, uint64_t amount)
{
    RWLOCK_READ_DEFER(&lock);

    node_t* node = path_traverse_node(path, &root.node);
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
    if (file->resource->ops->cleanup != NULL)
    {
        file->resource->ops->cleanup(file->resource, file);
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

    rwlock_init(&lock);

    printf("sysfs: init");
}

void sysfs_mount_to_vfs(void)
{
    ASSERT_PANIC(vfs_mount("sys", &sysfs) != ERR);
}

static node_t* sysfs_traverse_and_allocate(const char* path)
{
    path_t parsedPath;
    if (path_init(&parsedPath, path, NULL) == ERR)
    {
        return NULL;
    }
    if (parsedPath.volume[0] != '\0')
    {
        return NULL;
    }

    node_t* parent = &root.node;
    const char* name;
    PATH_FOR_EACH(name, &parsedPath)
    {
        node_t* child = node_find(parent, name);
        if (child == NULL)
        {
            sysdir_t* dir = malloc(sizeof(sysdir_t));
            if (dir == NULL)
            {
                return NULL;
            }

            node_init(&dir->node, name, SYSFS_DIR);
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
    }

    return parent;
}

sysdir_t* sysdir_new(const char* path, const char* dirname, sysdir_on_free_t onFree, void* private)
{
    RWLOCK_WRITE_DEFER(&lock);

    node_t* parent = sysfs_traverse_and_allocate(path);
    if (parent == NULL)
    {
        return NULL;
    }

    if (node_find(parent, dirname) != NULL)
    {
        return ERRPTR(EEXIST);
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

void sysdir_free(sysdir_t* dir)
{
    RWLOCK_WRITE_DEFER(&lock);

    node_t* node;
    node_t* temp;
    LIST_FOR_EACH_SAFE(node, temp, &dir->node.children, entry)
    {
        ASSERT_PANIC(node->type == SYSFS_RESOURCE);
        resource_t* resource = NODE_CONTAINER(node, resource_t, node);

        atomic_store(&resource->hidden, true);
        node_remove(&resource->node);
        resource_deref(resource);
    }

    node_remove(&dir->node);
    sysdir_deref(dir);
}

uint64_t sysdir_add(sysdir_t* dir, const char* filename, const resource_ops_t* ops, void* private)
{
    RWLOCK_WRITE_DEFER(&lock);

    if (node_find(&dir->node, filename) != NULL)
    {
        return ERROR(EEXIST);
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

    node_push(&dir->node, &resource->node); // Reference
    return 0;
}

resource_t* resource_new(const char* path, const char* filename, const resource_ops_t* ops, void* private)
{
    RWLOCK_WRITE_DEFER(&lock);

    node_t* parent = sysfs_traverse_and_allocate(path);
    if (parent == NULL)
    {
        return NULL;
    }

    if (node_find(parent, filename) != NULL)
    {
        return ERRPTR(EEXIST);
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

    node_push(parent, &resource->node); // First reference
    return resource;                    // Second reference
}

void resource_free(resource_t* resource)
{
    RWLOCK_WRITE_DEFER(&lock);

    atomic_store(&resource->hidden, true);
    node_remove(&resource->node);
    resource_deref(resource);
}
