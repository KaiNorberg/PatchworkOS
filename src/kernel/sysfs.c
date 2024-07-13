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
    list_entry_init(&system->base);
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
    system_t* parent = sysfs_traverse(path);
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

static uint64_t sysfs_open(file_t* file, const char* path)
{
    LOCK_GUARD(&lock);

    resource_t* resource = sysfs_find_resource(path);
    if (resource == NULL)
    {
        return ERROR(EPATH);
    }

    file->resource = resource;
    file->private = resource->private;
    atomic_fetch_add(&file->resource->openFiles, 1);

    return resource->ops->open != NULL ? resource->ops->open(file, path) : 0;
}

static void sysfs_cleanup(file_t* file)
{
    if (file->resource->ops->cleanup != NULL)
    {
        file->resource->ops->cleanup(file);
    }

    if (atomic_fetch_sub(&file->resource->openFiles, 1) <= 1 && atomic_load(&file->resource->dead))
    {
        resource_free(file->resource);
    }
}

#define SYSFS_OPERATION(name, file, ...) \
    ({ \
        uint64_t result; \
        if (atomic_load(&file->resource->dead)) \
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
        if (atomic_load(&file->resource->dead)) \
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

static uint64_t sysfs_seek(file_t* file, int64_t offset, uint8_t origin)
{
    return SYSFS_OPERATION(seek, file, offset, origin);
}

static uint64_t sysfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    return SYSFS_OPERATION(ioctl, file, request, argp, size);
}

static uint64_t sysfs_flush(file_t* file, const void* buffer, uint64_t count, const rect_t* rect)
{
    return SYSFS_OPERATION(flush, file, buffer, count, rect);
}

static void* sysfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    return SYSFS_OPERATION_PTR(mmap, file, address, length, prot);
}

static uint64_t sysfs_status(file_t* file, poll_file_t* pollFile)
{
    return SYSFS_OPERATION(status, file, pollFile);
}

static uint64_t sysfs_stat(volume_t* volume, const char* path, stat_t* buffer)
{
    LOCK_GUARD(&lock);

    buffer->size = 0;

    system_t* parent = sysfs_traverse(path);
    if (parent == NULL)
    {
        return ERR;
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

static file_ops_t fileOps = {
    .open = sysfs_open,
    .cleanup = sysfs_cleanup,
    .read = sysfs_read,
    .write = sysfs_write,
    .seek = sysfs_seek,
    .ioctl = sysfs_ioctl,
    .flush = sysfs_flush,
    .mmap = sysfs_mmap,
    .status = sysfs_status,
};

static volume_ops_t volumeOps = {
    .stat = sysfs_stat,
};

static uint64_t sysfs_mount(const char* label)
{
    return vfs_attach_simple(label, &volumeOps, &fileOps);
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

resource_t* sysfs_expose(const char* path, const char* filename, const file_ops_t* ops, void* private, resource_delete_t delete)
{
    LOCK_GUARD(&lock);

    system_t* system = root;
    const char* name = name_first(path);
    while (name != NULL)
    {
        system_t* next = system_find_system(system, name);
        if (next == NULL)
        {
            next = system_new(name);
            list_push(&system->systems, next);
        }

        system = next;
        name = name_next(name);
    }

    resource_t* resource = malloc(sizeof(resource_t));
    list_entry_init(&resource->base);
    resource->system = system;
    strcpy(resource->name, filename);
    resource->ops = ops;
    resource->private = private;
    resource->delete = delete;
    atomic_init(&resource->openFiles, 0);
    atomic_init(&resource->dead, false);

    list_push(&system->resources, resource);
    return resource;
}

uint64_t sysfs_hide(resource_t* resource)
{
    lock_acquire(&lock);
    list_remove(resource);
    atomic_store(&resource->dead, true);
    lock_release(&lock);

    if (atomic_load(&resource->openFiles) == 0)
    {
        resource_free(resource);
    }
    return 0;
}
