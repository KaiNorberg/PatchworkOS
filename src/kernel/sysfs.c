#include "sysfs.h"

#include "debug.h"
#include "lock.h"
#include "sched.h"
#include "tty.h"

#include <stdlib.h>
#include <string.h>

static Filesystem sysfs;

static System* root;
static Lock lock;

static System* system_new(const char* name)
{
    System* system = malloc(sizeof(System));
    list_entry_init(&system->base);
    name_copy(system->name, name);
    list_init(&system->resources);
    list_init(&system->systems);

    return system;
}

static System* system_find_system(System* parent, const char* name)
{
    System* system;
    LIST_FOR_EACH(system, &parent->systems)
    {
        if (name_compare(system->name, name))
        {
            return system;
        }
    }

    return NULL;
}

static Resource* system_find_resource(System* parent, const char* name)
{
    Resource* resource;
    LIST_FOR_EACH(resource, &parent->resources)
    {
        if (name_compare(resource->name, name))
        {
            return resource;
        }
    }

    return NULL;
}

static System* sysfs_traverse(const char* path)
{
    System* system = root;
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

static Resource* sysfs_find_resource(const char* path)
{
    System* parent = sysfs_traverse(path);
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

static uint64_t sysfs_open(Volume* volume, File* file, const char* path)
{
    LOCK_GUARD(&lock);

    Resource* resource = sysfs_find_resource(path);
    if (resource == NULL)
    {
        return ERROR(EPATH);
    }

    return resource->open(resource, file);
}

static uint64_t sysfs_stat(Volume* volume, const char* path, stat_t* buffer)
{
    buffer->size = 0;

    System* parent = sysfs_traverse(path);
    if (parent == NULL)
    {
        return ERR;
    }

    const char* name = vfs_basename(path);
    if (system_find_resource(parent, name) != NULL)
    {
        buffer->type = STAT_FILE;
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

static uint64_t sysfs_mount(Volume* volume)
{
    volume->open = sysfs_open;
    volume->stat = sysfs_stat;

    return 0;
}

void resource_init(Resource* resource, const char* name, uint64_t (*open)(Resource* resource, File* file),
    void (*delete)(Resource* resource))
{
    list_entry_init(&resource->base);
    resource->system = NULL;
    atomic_init(&resource->ref, 1);
    name_copy(resource->name, name);
    resource->open = open;
    resource->delete = delete;
}

Resource* resource_ref(Resource* resource)
{
    atomic_fetch_add(&resource->ref, 1);
    return resource;
}

void resource_unref(Resource* resource)
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
    tty_start_message("Sysfs initializing");

    root = system_new("root");
    lock_init(&lock);

    memset(&sysfs, 0, sizeof(Filesystem));
    sysfs.name = "sysfs";
    sysfs.mount = sysfs_mount;

    if (vfs_mount("sys", &sysfs) == ERR)
    {
        tty_print("Failed to mount sysfs");
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void sysfs_expose(Resource* resource, const char* path)
{
    LOCK_GUARD(&lock);

    System* system = root;
    const char* name = name_first(path);
    while (name != NULL)
    {
        System* next = system_find_system(system, name);
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

void sysfs_hide(Resource* resource)
{
    LOCK_GUARD(&lock);

    resource->system = NULL;
    list_remove(resource);
    resource_unref(resource);
}