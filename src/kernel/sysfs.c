#include "sysfs.h"

#include "debug.h"
#include "lock.h"
#include "sched.h"

#include <stdlib.h>
#include <string.h>

static fs_t sysfs;

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

static uint64_t sysfs_open(volume_t* volume, file_t* file, const char* path)
{
    LOCK_GUARD(&lock);

    resource_t* resource = sysfs_find_resource(path);
    if (resource == NULL)
    {
        return ERROR(EPATH);
    }

    return resource->open(resource, file);
}

static uint64_t sysfs_stat(volume_t* volume, const char* path, stat_t* buffer)
{
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

static uint64_t sysfs_mount(volume_t* volume)
{
    volume->open = sysfs_open;
    volume->stat = sysfs_stat;

    return 0;
}

void resource_init(resource_t* resource, const char* name, uint64_t (*open)(resource_t* resource, file_t* file),
    void (*delete)(resource_t* resource))
{
    list_entry_init(&resource->base);
    resource->system = NULL;
    atomic_init(&resource->ref, 1);
    name_copy(resource->name, name);
    resource->open = open;
    resource->delete = delete;
}

resource_t* resource_ref(resource_t* resource)
{
    atomic_fetch_add(&resource->ref, 1);
    return resource;
}

void resource_unref(resource_t* resource)
{
    if (atomic_fetch_sub(&resource->ref, 1) <= 1)
    {
        if (resource->delete != NULL)
        {
            resource->delete (resource);
        }
        else
        {
            debug_panic("Attempt to delete undeletable resource");
        }
    }
}

void sysfs_init(void)
{
    root = system_new("root");
    lock_init(&lock);

    memset(&sysfs, 0, sizeof(fs_t));
    sysfs.name = "sysfs";
    sysfs.mount = sysfs_mount;

    DEBUG_ASSERT(vfs_mount("sys", &sysfs) != ERR, "mount fail");
}

void sysfs_expose(resource_t* resource, const char* path)
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

    resource->system = system;
    list_push(&system->resources, resource);
}

void sysfs_hide(resource_t* resource)
{
    LOCK_GUARD(&lock);

    resource->system = NULL;
    list_remove(resource);
    resource_unref(resource);
}
