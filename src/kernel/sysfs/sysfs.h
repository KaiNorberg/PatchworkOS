#pragma once

#include "defs/defs.h"
#include "list/list.h"
#include "vfs/vfs.h"

typedef struct
{
    ListEntry base;
    char name[CONFIG_MAX_NAME];
    List resources;
    Lock lock;
} System;

typedef struct Resource
{
    ListEntry base;
    System* system;
    char name[CONFIG_MAX_NAME];
    void (*cleanup)(struct Resource*);
    uint64_t (*read)(File*, void*, uint64_t);
    uint64_t (*write)(File*, const void*, uint64_t);
    uint64_t (*seek)(File*, int64_t, uint8_t);
} Resource;

void resource_init(Resource* resource);

void sysfs_init();

void sysfs_expose(Resource* resource, const char* path, const char* name);

void sysfs_hide(Resource* resource);
