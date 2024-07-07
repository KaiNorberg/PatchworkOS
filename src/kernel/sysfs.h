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
    char name[CONFIG_MAX_NAME];
    file_ops_t ops;
} resource_t;

void sysfs_init(void);

void sysfs_expose(const char* path, const char* filename, const file_ops_t* ops);
