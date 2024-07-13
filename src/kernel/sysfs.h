#pragma once

#include <stdatomic.h>
#include <sys/list.h>

#include "defs.h"
#include "vfs.h"

typedef struct system
{
    list_entry_t base;
    char name[CONFIG_MAX_NAME];
    list_t resources;
    list_t systems;
} system_t;

typedef void (*resource_delete_t)(void*);

typedef struct resource
{
    list_entry_t base;
    system_t* system;
    char name[CONFIG_MAX_NAME];
    const file_ops_t* ops;
    void* private;
    resource_delete_t delete;
    atomic_uint64_t openFiles;
    atomic_bool dead;
} resource_t;

void sysfs_init(void);

resource_t* sysfs_expose(const char* path, const char* filename, const file_ops_t* ops, void* private, resource_delete_t delete);

uint64_t sysfs_hide(resource_t* resource);
