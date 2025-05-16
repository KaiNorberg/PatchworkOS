#include "sysfs.h"

#include "utils/log.h"
#include "path.h"
#include "sync/rwlock.h"
#include "sched/sched.h"
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

static void syshdr_init(syshdr_t* header, const char* name, uint64_t type)
{
    node_init(&header->node, name, type);
    atomic_init(&header->hidden, false);
    atomic_init(&header->ref, 1);
}

static sysdir_t* sysdir_ref(sysdir_t* dir)
{
    atomic_fetch_add(&dir->header.ref, 1);
    return dir;
}

static void sysdir_deref(sysdir_t* dir)
{
    if (atomic_fetch_sub(&dir->header.ref, 1) <= 1)
    {
        if (dir->onFree != NULL)
        {
            dir->onFree(dir);
        }
    }
}

static sysobj_t* sysobj_ref(sysobj_t* sysobj)
{
    atomic_fetch_add(&sysobj->header.ref, 1);
    return sysobj;
}

static void sysobj_deref(sysobj_t* sysobj)
{
    if (atomic_fetch_sub(&sysobj->header.ref, 1) <= 1)
    {
        if (sysobj->onFree != NULL)
        {
            sysobj->onFree(sysobj);
        }

        sysdir_deref(sysobj->dir);
    }
}

static uint64_t sysfs_readdir(file_t* file, stat_t* infos, uint64_t amount)
{
    RWLOCK_READ_DEFER(&lock);
    sysdir_t* sysdir = CONTAINER_OF(file->syshdr, sysdir_t, header);

    uint64_t index = 0;
    uint64_t total = 0;

    node_t* child;
    LIST_FOR_EACH(child, &sysdir->header.node.children, entry)
    {
        stat_t info = {0};
        strcpy(info.name, child->name);
        info.type = child->type == SYSFS_OBJ ? STAT_FILE : STAT_DIR;
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
    node_t* node = path_traverse_node(path, &root.header.node);
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

        sysobj_t* sysobj = sysobj_ref(CONTAINER_OF(node, sysobj_t, header.node));
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

        file->syshdr = &sysobj->header;
        return file;
    }
    else
    {
        if (!(path->flags & PATH_DIRECTORY))
        {
            rwlock_read_release(&lock);
            return ERRPTR(ENOTDIR);
        }

        sysdir_t* sysdir = sysdir_ref(CONTAINER_OF(node, sysdir_t, header.node));
        rwlock_read_release(&lock);

        file_t* file = file_new(volume, path, PATH_DIRECTORY);
        if (file == NULL)
        {
            sysdir_deref(sysdir);
            return NULL;
        }
        file->ops = &dirOps;

        file->syshdr = &sysdir->header;
        return file;
    }
}

static uint64_t sysfs_open2(volume_t* volume, const path_t* path, file_t* files[2])
{
    RWLOCK_READ_DEFER(&lock);
    node_t* node = path_traverse_node(path, &root.header.node);
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
    sysobj_t* sysobj = sysobj_ref(CONTAINER_OF(node, sysobj_t, header.node)); // First ref

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

    files[0]->syshdr = &sysobj->header; // First ref
    files[1]->syshdr = &sysobj_ref(sysobj)->header;
    return 0;
}

static uint64_t sysfs_stat(volume_t* volume, const path_t* path, stat_t* stat)
{
    RWLOCK_READ_DEFER(&lock);

    node_t* node = path_traverse_node(path, &root.header.node);
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
    if (file->syshdr->node.type == SYSFS_DIR)
    {
        sysdir_t* dir = CONTAINER_OF(file->syshdr, sysdir_t, header);
        sysdir_deref(dir);
    }
    else
    {
        sysobj_t* sysobj = CONTAINER_OF(file->syshdr, sysobj_t, header);

        if (sysobj->ops->cleanup != NULL)
        {
            sysobj->ops->cleanup(sysobj, file);
        }

        sysobj_deref(sysobj);
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
    syshdr_init(&root.header, "root", SYSFS_DIR);
    root.private = NULL;
    root.onFree = NULL;

    rwlock_init(&lock);

    printf("sysfs: init\n");
}

void sysfs_mount_to_vfs(void)
{
    ASSERT_PANIC(vfs_mount("sys", &sysfs) != ERR);
}

uint64_t sysfs_start_op(file_t* file)
{
    ASSERT_PANIC(file->syshdr != NULL);
    if (atomic_load(&file->syshdr->hidden) == true)
    {

        return ERROR(ENOOBJ);
    }

    return 0;
}

void sysfs_end_op(file_t* file)
{

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

    node_t* parent = &root.header.node;
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

            syshdr_init(&dir->header, name, SYSFS_DIR);
            dir->private = NULL;
            dir->onFree = NULL;
            node_push(parent, &dir->header.node);

            child = &dir->header.node;
        }
        if (child->type != SYSFS_DIR)
        {
            return ERRPTR(ENOTDIR);
        }

        parent = child;
    }

    return parent;
}

uint64_t sysdir_init(sysdir_t* dir, const char* path, const char* dirname, void* private)
{
    if (!path_valid_name(dirname))
    {
        return ERROR(EINVAL);
    }

    RWLOCK_WRITE_DEFER(&lock);

    node_t* parent = sysfs_traverse_and_alloc(path);
    if (parent == NULL)
    {
        return ERR;
    }

    if (node_find(parent, dirname) != NULL)
    {
        return ERROR(EEXIST);
    }

    syshdr_init(&dir->header, dirname, SYSFS_DIR);
    dir->private = private;
    dir->onFree = NULL;
    node_push(parent, &dir->header.node);

    return 0;
}

void sysdir_deinit(sysdir_t* dir, sysdir_on_free_t onFree)
{
    dir->onFree = onFree;

    rwlock_write_acquire(&lock);
    node_t* node;
    node_t* temp;
    LIST_FOR_EACH_SAFE(node, temp, &dir->header.node.children, entry)
    {
        ASSERT_PANIC(node->type == SYSFS_OBJ);
        sysobj_t* sysobj = CONTAINER_OF(node, sysobj_t, header.node);

        atomic_store(&sysobj->header.hidden, true);
        node_remove(&sysobj->header.node);
        sysobj_deref(sysobj);
    }
    node_remove(&dir->header.node);
    rwlock_write_release(&lock);

    sysdir_deref(dir);
}

uint64_t sysobj_init(sysobj_t* sysobj, sysdir_t* dir, const char* filename, const sysobj_ops_t* ops, void* private)
{
    if (!path_valid_name(filename))
    {
        return ERROR(EINVAL);
    }

    RWLOCK_WRITE_DEFER(&lock);

    if (node_find(&dir->header.node, filename) != NULL)
    {
        return ERROR(EEXIST);
    }

    syshdr_init(&sysobj->header, filename, SYSFS_OBJ);
    sysobj->private = private;
    sysobj->ops = ops;
    sysobj->onFree = NULL;
    sysobj->dir = sysdir_ref(dir);

    node_push(&dir->header.node, &sysobj->header.node); // Reference
    return 0;
}

uint64_t sysobj_init_path(sysobj_t* sysobj, const char* path, const char* filename, const sysobj_ops_t* ops, void* private)
{
    if (!path_valid_name(filename))
    {
        return ERROR(EINVAL);
    }

    RWLOCK_WRITE_DEFER(&lock);

    node_t* parent = sysfs_traverse_and_alloc(path);
    if (parent == NULL)
    {
        return ERR;
    }

    if (node_find(parent, filename) != NULL)
    {
        return ERROR(EEXIST);
    }

    syshdr_init(&sysobj->header, filename, SYSFS_OBJ);
    sysobj->private = private;
    sysobj->ops = ops;
    sysobj->dir = sysdir_ref(CONTAINER_OF(parent, sysdir_t, header.node));

    node_push(parent, &sysobj->header.node);
    sysobj_ref(sysobj);
    return 0;
}

void sysobj_deinit(sysobj_t* sysobj, sysobj_on_free_t onFree)
{
    sysobj->onFree = onFree;

    rwlock_write_acquire(&lock);
    atomic_store(&sysobj->header.hidden, true);
    node_remove(&sysobj->header.node);
    rwlock_write_release(&lock);

    sysobj_deref(sysobj);
}
