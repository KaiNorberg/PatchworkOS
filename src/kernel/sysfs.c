#include "sysfs.h"

#include "lock.h"
#include "log.h"
#include "sched.h"
#include "sys/list.h"
#include "vfs.h"

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

    file->ops = resource->ops;
    if (file->ops.open != NULL)
    {
        return file->ops.open(file, path);
    }
    return 0;
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

static volume_ops_t volumeOps = {
    .stat = sysfs_stat
};

static file_ops_t fileOps = {
    .open = sysfs_open,
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

void sysfs_expose(const char* path, const char* filename, const file_ops_t* ops)
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
    resource->ops = *ops;

    list_push(&system->resources, resource);
}
