#pragma once

#include <stdatomic.h>

#include "defs.h"
#include "list.h"
#include "vfs.h"

typedef struct System
{
    ListEntry base;
    char name[CONFIG_MAX_NAME];
    List resources;
    List systems;
} System;

typedef struct Resource
{
    ListEntry base;
    System* system;
    _Atomic(uint64_t) ref;
    char name[CONFIG_MAX_NAME];
    void (*delete)(struct Resource*);
    FileMethods methods;
} Resource;

static inline void resource_init(Resource* resource, const char* name)
{
    list_entry_init(&resource->base);
    resource->system = NULL;
    atomic_init(&resource->ref, 1);
    vfs_copy_name(resource->name, name);
    resource->delete = NULL;
    resource->methods = (FileMethods){};
}

void sysfs_init();

void sysfs_expose(Resource* resource, const char* path);

void sysfs_hide(Resource* resource);
