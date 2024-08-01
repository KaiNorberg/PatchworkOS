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

static system_t* root;
static lock_t lock;

static system_t* system_new(const char* name)
{
    system_t* system = malloc(sizeof(system_t));
    list_entry_init(&system->entry);
    name_copy(system->name, name);
    list_init(&system->resources);
    list_init(&system->systems);

    return system;
}

static system_t* system_find_system(system_t* parent, const char* name)
{
    system_t* system;
    LIST_FOR_EACH(system, &parent->systems)
    {
        if (name_compare(system->name, name))
        {
            return system;
        }
    }

    return NULL;
}

static resource_t* system_find_resource(system_t* parent, const char* name)
{
    resource_t* resource;
    LIST_FOR_EACH(resource, &parent->resources)
    {
        if (name_compare(resource->name, name))
        {
            return resource;
        }
    }

    return NULL;
}

static void resource_free(resource_t* resource)
{
    if (resource->delete != NULL)
    {
        resource->delete (resource); // Why is clang-format doing this?
    }

    free(resource);
}

static system_t* sysfs_traverse(const char* path)
{
    system_t* system = root;
    const char* name = name_first(path);
    while (name != NULL)
    {
        system = system_find_system(system, name);
        if (system == NULL)
        {
            return NULL;
        }

        name = name_next(name);
    }

    return system;
}

static system_t* sysfs_traverse_parent(const char* path)
{
    system_t* system = root;
    const char* name = dir_name_first(path);
    while (name != NULL)
    {
        system = system_find_system(system, name);
        if (system == NULL)
        {
            return NULL;
        }

        name = dir_name_next(name);
    }

    return system;
}

static resource_t* sysfs_find_resource(const char* path)
{
    system_t* parent = sysfs_traverse_parent(path);
    if (parent == NULL)
    {
        return NULL;
    }

    const char* name = vfs_basename(path);
    if (name == NULL)
    {
        return NULL;
    }

    return system_find_resource(parent, name);
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
            result = NULLPTR(ENORES); \
        } \
        else if (file->resource->ops->name != NULL) \
        { \
            result = file->resource->ops->name(file __VA_OPT__(, ) __VA_ARGS__); \
        } \
        else \
        { \
            result = NULLPTR(EACCES); \
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

    resource_t* resource = sysfs_find_resource(path);
    if (resource == NULL)
    {
        return NULLPTR(EPATH);
    }

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

static uint64_t sysfs_stat(volume_t* volume, const char* path, stat_t* buffer)
{
    LOCK_GUARD(&lock);

    buffer->size = 0;

    system_t* parent = sysfs_traverse_parent(path);
    if (parent == NULL)
    {
        return ERROR(EPATH);
    }

    const char* name = vfs_basename(path);
    if (system_find_resource(parent, name) != NULL)
    {
        buffer->type = STAT_RES;
    }
    else if (system_find_system(parent, name) != NULL)
    {
        buffer->type = STAT_DIR;
    }
    else
    {
        return ERROR(EPATH);
    }

    return 0;
}

static uint64_t sysfs_listdir(volume_t* volume, const char* path, dir_entry_t* entries, uint64_t amount)
{
    LOCK_GUARD(&lock);

    system_t* parent = sysfs_traverse(path);
    if (parent == NULL)
    {
        return ERROR(EPATH);
    }

    uint64_t index = 0;
    uint64_t total = 0;

    system_t* system;
    LIST_FOR_EACH(system, &parent->systems)
    {
        dir_entry_t entry = {0};
        strcpy(entry.name, system->name);
        entry.type = STAT_DIR;

        dir_entry_push(entries, amount, &index, &total, &entry);
    }

    resource_t* resource;
    LIST_FOR_EACH(resource, &parent->resources)
    {
        dir_entry_t entry = {0};
        strcpy(entry.name, resource->name);
        entry.type = STAT_RES;

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
    root = system_new("root");
    lock_init(&lock);

    LOG_ASSERT(vfs_mount("sys", &sysfs) != ERR, "mount fail");

    log_print("sysfs: init");
}

resource_t* sysfs_expose(const char* path, const char* filename, const file_ops_t* ops, void* private, resource_open_t open,
    resource_delete_t delete)
{
    LOCK_GUARD(&lock);

    system_t* system = root;
    const char* name = name_first(path);
    while (name != NULL)
    {
        system_t* child = system_find_system(system, name);
        if (child == NULL)
        {
            child = system_new(name);
            list_push(&system->systems, child);
        }

        system = child;
        name = name_next(name);
    }

    resource_t* resource = malloc(sizeof(resource_t));
    list_entry_init(&resource->entry);
    resource->system = system;
    strcpy(resource->name, filename);
    resource->ops = ops;
    resource->private = private;
    resource->open = open;
    resource->delete = delete;
    atomic_init(&resource->ref, 1);
    atomic_init(&resource->hidden, false);

    list_push(&system->resources, resource);
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
