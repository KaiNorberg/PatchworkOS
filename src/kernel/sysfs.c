#include "sysfs.h"

#include "lock.h"
#include "log.h"
#include "sched.h"
#include "sys/list.h"
#include "vfs.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static node_t root;
static lock_t lock;

static void resource_free(resource_t* resource)
{
    if (resource->delete != NULL)
    {
        resource->delete (resource); // Why is clang-format doing this?
    }

    node_remove(&resource->node);
    free(resource);
}

#define SYSFS_OPERATION(name, file, ...) \
    ({ \
        uint64_t result; \
        if (atomic_load(&file->resource->hidden)) \
        { \
            result = ERROR(ENORES); \
        } \
        else if (file->resource->ops->name != NULL) \
        { \
            result = file->resource->ops->name(file __VA_OPT__(, ) __VA_ARGS__); \
        } \
        else \
        { \
            result = ERROR(EACCES); \
        } \
        result; \
    })

#define SYSFS_OPERATION_PTR(name, file, ...) \
    ({ \
        void* result; \
        if (atomic_load(&file->resource->hidden)) \
        { \
            result = ERRPTR(ENORES); \
        } \
        else if (file->resource->ops->name != NULL) \
        { \
            result = file->resource->ops->name(file __VA_OPT__(, ) __VA_ARGS__); \
        } \
        else \
        { \
            result = ERRPTR(EACCES); \
        } \
        result; \
    })

static uint64_t sysfs_read(file_t* file, void* buffer, uint64_t count)
{
    return SYSFS_OPERATION(read, file, buffer, count);
}

static uint64_t sysfs_write(file_t* file, const void* buffer, uint64_t count)
{
    return SYSFS_OPERATION(write, file, buffer, count);
}

static uint64_t sysfs_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    return SYSFS_OPERATION(seek, file, offset, origin);
}

static uint64_t sysfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    return SYSFS_OPERATION(ioctl, file, request, argp, size);
}

static uint64_t sysfs_flush(file_t* file, const pixel_t* buffer, uint64_t size, const rect_t* rect)
{
    return SYSFS_OPERATION(flush, file, buffer, size, rect);
}

static void* sysfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    return SYSFS_OPERATION_PTR(mmap, file, address, length, prot);
}

static uint64_t sysfs_status(file_t* file, poll_file_t* pollFile)
{
    return SYSFS_OPERATION(status, file, pollFile);
}

static void sysfs_cleanup(file_t* file)
{
    if (file->resource->ops->cleanup != NULL)
    {
        file->resource->ops->cleanup(file);
    }

    if (atomic_fetch_sub(&file->resource->ref, 1) <= 1)
    {
        resource_free(file->resource);
    }
}

static file_ops_t fileOps = {
    .read = sysfs_read,
    .write = sysfs_write,
    .seek = sysfs_seek,
    .ioctl = sysfs_ioctl,
    .flush = sysfs_flush,
    .mmap = sysfs_mmap,
    .status = sysfs_status,
    .cleanup = sysfs_cleanup,
};

static file_t* sysfs_open(volume_t* volume, const char* path)
{
    LOCK_GUARD(&lock);

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

    file_t* file = file_new(volume);
    file->private = resource->private;
    file->ops = resource->ops;
    file->resource = resource;

    if (resource->open != NULL && resource->open(resource, file) == ERR)
    {
        return NULL;
    }

    atomic_fetch_add(&file->resource->ref, 1);

    return file;
}

static uint64_t sysfs_stat(volume_t* volume, const char* path, stat_t* stat)
{
    LOCK_GUARD(&lock);

    node_t* node = node_traverse(&root, path, VFS_NAME_SEPARATOR);
    if (node == NULL)
    {
        return ERROR(EPATH);
    }

    stat->size = 0;
    stat->type = node->type == SYSFS_RESOURCE ? STAT_RES : STAT_DIR;

    return 0;
}

static uint64_t sysfs_listdir(volume_t* volume, const char* path, dir_entry_t* entries, uint64_t amount)
{
    LOCK_GUARD(&lock);

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
    .open = sysfs_open,
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

    LOG_ASSERT(vfs_mount("sys", &sysfs) != ERR, "mount fail");

    log_print("sysfs: init");
}

resource_t* sysfs_expose(const char* path, const char* filename, const file_ops_t* ops, void* private, resource_open_t open,
    resource_delete_t delete)
{
    LOCK_GUARD(&lock);

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
    resource->open = open;
    resource->delete = delete;
    atomic_init(&resource->ref, 1);
    atomic_init(&resource->hidden, false);
    node_push(parent, &resource->node);

    return resource;
}

uint64_t sysfs_hide(resource_t* resource)
{
    lock_acquire(&lock);
    list_remove(resource);
    lock_release(&lock);

    atomic_store(&resource->hidden, true);
    if (atomic_fetch_sub(&resource->ref, 1) <= 1)
    {
        resource_free(resource);
    }

    return 0;
}
