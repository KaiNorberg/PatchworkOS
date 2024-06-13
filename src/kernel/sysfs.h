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
    void (*delete)(struct Resource* resource);
    uint64_t (*open)(struct Resource* resource, File* file);
} Resource;

void resource_init(
    Resource* resource, const char* name, uint64_t (*open)(Resource* resource, File* file), void (*delete)(Resource* resource));

Resource* resource_ref(Resource* resource);

void resource_unref(Resource* resource);

void sysfs_init(void);

void sysfs_expose(Resource* resource, const char* path);

void sysfs_hide(Resource* resource);