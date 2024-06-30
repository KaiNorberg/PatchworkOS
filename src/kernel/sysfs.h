#pragma once

#include <stdatomic.h>

#include "defs.h"
#include "vfs.h"

#include <sys/list.h>

typedef struct system
{
    list_entry_t base;
    char name[CONFIG_MAX_NAME];
    list_t resources;
    list_t systems;
} system_t;

typedef struct resource
{
    list_entry_t base;
    system_t* system;
    _Atomic(uint64_t) ref;
    char name[CONFIG_MAX_NAME];
    void (*delete)(struct resource* resource);
    uint64_t (*open)(struct resource* resource, file_t* file);
} resource_t;

void resource_init(resource_t* resource, const char* name, uint64_t (*open)(resource_t* resource, file_t* file),
    void (*delete)(resource_t* resource));

resource_t* resource_ref(resource_t* resource);

void resource_unref(resource_t* resource);

void sysfs_init(void);

void sysfs_expose(resource_t* resource, const char* path);

void sysfs_hide(resource_t* resource);
