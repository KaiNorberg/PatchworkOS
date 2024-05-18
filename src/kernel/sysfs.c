#include "sysfs.h"

#include <string.h>

#include "heap.h"
#include "sched.h"
#include "tty.h"
#include "lock.h"
#include "debug.h"

static Filesystem sysfs;

static System* root;
static Lock lock;

static Resource* resource_ref(Resource* resource)
{
    atomic_fetch_add(&resource->ref, 1);
    return resource;
}

static void resource_unref(Resource* resource)
{
    if (atomic_fetch_sub(&resource->ref, 1) <= 1)
    {
        if (resource->delete != NULL)
        {
            resource->delete(resource);
        }
        else
        {
            debug_panic("Attempt to delete undeletable resource");
        }
    }
}

static System* system_new(const char* name)
{
    System* system = kmalloc(sizeof(System));
    list_entry_init(&system->base);
    vfs_copy_name(system->name, name);
    list_init(&system->resources);
    list_init(&system->systems);

    return system;
}

static System* system_find_system(System* parent, const char* name)
{
    System* system;
    LIST_FOR_EACH(system, &parent->systems)
    {
        if (vfs_compare_names(system->name, name))
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
        if (vfs_compare_names(resource->name, name))
        {
            return resource;
        }
    }

    return NULL;
}

static System* sysfs_traverse(const char* path)
{
    System* system = root;
    const char* name = vfs_first_dir(path);
    while (name != NULL)
    {
        system = system_find_system(system, name);
        if (system == NULL)
        {
            return NULL;
        }

        name = vfs_next_dir(name);
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

static void sysfs_cleanup(File* file)
{
    Resource* resource = file->internal;
    resource_unref(resource);
}

static uint64_t sysfs_open(Volume* volume, File* file, const char* path)
{
    LOCK_GUARD(&lock);

    Resource* resource = sysfs_find_resource(path);
    if (resource == NULL)
    {
        return ERROR(EPATH);
    }

    file->cleanup = sysfs_cleanup;
    file->methods = resource->methods;
    file->internal = resource_ref(resource);

    return 0;
}

static uint64_t sysfs_mount(Volume* volume)
{
    volume->open = sysfs_open;

    return 0;
}

void sysfs_init(void)
{
    tty_start_message("Sysfs initializing");

    root = system_new("root");
    lock_init(&lock);

    memset(&sysfs, 0, sizeof(Filesystem));
    sysfs.name = "sysfs";
    sysfs.mount = sysfs_mount;

    if (vfs_mount('A', &sysfs) == ERR)
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
    const char* name = vfs_first_name(path);
    while (name != NULL)
    {
        System* next = system_find_system(system, name);
        if (next == NULL)
        {
            next = system_new(name);
            list_push(&system->systems, next);
        }

        system = next;
        name = vfs_next_name(name);
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