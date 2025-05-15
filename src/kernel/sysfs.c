#include "sysfs.h"

#include "log.h"
#include "path.h"
#include "rwlock.h"
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

static sysobj_t* sysobj_ref(sysobj_t* sysobj)
{
    atomic_fetch_add(&sysobj->ref, 1);
    return sysobj;
}

static void sysobj_deref(sysobj_t* sysobj)
{
    if (atomic_fetch_sub(&sysobj->ref, 1) <= 1)
    {
        if (sysobj->ops->onFree != NULL)
        {
            sysobj->ops->onFree(sysobj);
        }

        sysdir_deref(sysobj->dir);

        free(sysobj);
    }
}

static uint64_t sysfs_readdir(file_t* file, stat_t* infos, uint64_t amount)
{
    RWLOCK_READ_DEFER(&lock);
    sysdir_t* sysdir = file->private;

    uint64_t index = 0;
    uint64_t total = 0;

    node_t* child;
    LIST_FOR_EACH(child, &sysdir->node.children, entry)
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
    .readdir = sysfs_readdir,
};

static file_t* sysfs_open(volume_t* volume, const path_t* path)
{
    rwlock_read_acquire(&lock);
    node_t* node = path_traverse_node(path, &root.node);
    if (node == NULL)
    {
        rwlock_read_release(&lock);
        return ERRPTR(EPATH);
    }

    if (node->type == SYSFS_OBJ)
    {
        if (path->flags & PATH_DIRECTORY)
        {
            rwlock_read_release(&lock);
            return ERRPTR(EISDIR);
        }

        sysobj_t* sysobj = sysobj_ref(NODE_CONTAINER(node, sysobj_t, node));
        rwlock_read_release(&lock);

        if (sysobj->ops->open == NULL)
        {
            sysobj_deref(sysobj);
            return ERRPTR(ENOOP);
        }

        file_t* file = sysobj->ops->open(volume, path, sysobj);
        if (file == NULL)
        {
            sysobj_deref(sysobj);
            return NULL;
        }

        file->sysobj = sysobj;
        return file;
    }
    else
    {
        if (!(path->flags & PATH_DIRECTORY))
        {
            rwlock_read_release(&lock);
            return ERRPTR(ENOTDIR);
        }

        sysdir_t* sysdir = sysdir_ref(NODE_CONTAINER(node, sysdir_t, node));
        rwlock_read_release(&lock);

        file_t* file = file_new(volume, path, PATH_DIRECTORY);
        if (file == NULL)
        {
            sysdir_deref(sysdir);
            return NULL;
        }
        file->private = sysdir;
        file->ops = &dirOps;

        return file;
    }
}

static uint64_t sysfs_open2(volume_t* volume, const path_t* path, file_t* files[2])
{
    RWLOCK_READ_DEFER(&lock);
    node_t* node = path_traverse_node(path, &root.node);
    if (node == NULL)
    {
        return ERROR(EPATH);
    }
    else if (node->type != SYSFS_OBJ)
    {
        return ERROR(EISDIR);
    }
    else if (path->flags & PATH_DIRECTORY)
    {
        return ERROR(EINVAL);
    }
    sysobj_t* sysobj = sysobj_ref(NODE_CONTAINER(node, sysobj_t, node)); // First ref

    if (sysobj->ops->open2 == NULL)
    {
        sysobj_deref(sysobj);
        return ERROR(ENOOP);
    }

    if (sysobj->ops->open2(volume, path, sysobj, files) == ERR)
    {
        sysobj_deref(sysobj);
        return ERR;
    }

    files[0]->sysobj = sysobj; // First ref
    files[1]->sysobj = sysobj_ref(sysobj);
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

    strcpy(stat->name, node->name);
    stat->type = node->type == SYSFS_OBJ ? STAT_FILE : STAT_DIR;
    stat->size = 0;

    return 0;
}

static void sysfs_cleanup(volume_t* volume, file_t* file)
{
    if (file->sysobj == NULL) // Is dir
    {
        sysdir_deref(file->private);
    }
    else
    {
        if (file->sysobj->ops->cleanup != NULL)
        {
            file->sysobj->ops->cleanup(file->sysobj, file);
        }

        sysobj_deref(file->sysobj);
    }
}

static volume_ops_t volumeOps = {
    .open = sysfs_open,
    .open2 = sysfs_open2,
    .stat = sysfs_stat,
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

    printf("sysfs: init\n");
}

void sysfs_mount_to_vfs(void)
{
    ASSERT_PANIC(vfs_mount("sys", &sysfs) != ERR);
}

static node_t* sysfs_traverse_and_alloc(const char* path)
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
    if (!path_valid_name(dirname))
    {
        return ERRPTR(EINVAL);
    }

    RWLOCK_WRITE_DEFER(&lock);

    node_t* parent = sysfs_traverse_and_alloc(path);
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
    rwlock_write_acquire(&lock);
    node_t* node;
    node_t* temp;
    LIST_FOR_EACH_SAFE(node, temp, &dir->node.children, entry)
    {
        ASSERT_PANIC(node->type == SYSFS_OBJ);
        sysobj_t* sysobj = NODE_CONTAINER(node, sysobj_t, node);

        atomic_store(&sysobj->hidden, true);
        node_remove(&sysobj->node);
        sysobj_deref(sysobj);
    }
    node_remove(&dir->node);
    rwlock_write_release(&lock);

    sysdir_deref(dir);
}

uint64_t sysdir_add(sysdir_t* dir, const char* filename, const sysobj_ops_t* ops, void* private)
{
    if (!path_valid_name(filename))
    {
        return ERROR(EINVAL);
    }

    RWLOCK_WRITE_DEFER(&lock);

    if (node_find(&dir->node, filename) != NULL)
    {
        return ERROR(EEXIST);
    }

    sysobj_t* sysobj = malloc(sizeof(sysobj_t));
    if (sysobj == NULL)
    {
        return ERR;
    }
    node_init(&sysobj->node, filename, SYSFS_OBJ);
    sysobj->private = private;
    sysobj->ops = ops;
    atomic_init(&sysobj->ref, 1);
    atomic_init(&sysobj->hidden, false);
    sysobj->dir = sysdir_ref(dir);

    node_push(&dir->node, &sysobj->node); // Reference
    return 0;
}

sysobj_t* sysobj_new(const char* path, const char* filename, const sysobj_ops_t* ops, void* private)
{
    if (!path_valid_name(filename))
    {
        return ERRPTR(EINVAL);
    }

    RWLOCK_WRITE_DEFER(&lock);

    node_t* parent = sysfs_traverse_and_alloc(path);
    if (parent == NULL)
    {
        return NULL;
    }

    if (node_find(parent, filename) != NULL)
    {
        return ERRPTR(EEXIST);
    }

    sysobj_t* sysobj = malloc(sizeof(sysobj_t));
    if (sysobj == NULL)
    {
        return NULL;
    }
    node_init(&sysobj->node, filename, SYSFS_OBJ);
    sysobj->private = private;
    sysobj->ops = ops;
    atomic_init(&sysobj->ref, 2);
    atomic_init(&sysobj->hidden, false);
    sysobj->dir = sysdir_ref(NODE_CONTAINER(parent, sysdir_t, node));

    node_push(parent, &sysobj->node); // First reference
    return sysobj;                    // Second reference
}

void sysobj_free(sysobj_t* sysobj)
{
    rwlock_write_acquire(&lock);
    atomic_store(&sysobj->hidden, true);
    node_remove(&sysobj->node);
    rwlock_write_release(&lock);

    sysobj_deref(sysobj);
}
