#pragma once

#include "defs/defs.h"
#include "tree/tree.h"
#include "lock/lock.h"
#include "vfs/vfs.h"

typedef struct
{
    ListEntry base;
    char name[CONFIG_MAX_NAME];
    List resources;
    List systems;
    Lock lock;
} System;

typedef struct Resource
{
    ListEntry base;
    System* system;
    char name[CONFIG_MAX_NAME];
    void (*cleanup)(struct Resource*);
    FileMethods methods;
} Resource;

void resource_init(Resource* resource);

void sysfs_init();

void sysfs_expose(Resource* resource, const char* path, const char* name);

void sysfs_hide(Resource* resource);
