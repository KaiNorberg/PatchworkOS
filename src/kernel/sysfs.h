#pragma once

#include <stdatomic.h>
#include <sys/node.h>

#include "defs.h"
#include "vfs.h"

#define SYSFS_RESOURCE 0
#define SYSFS_SYSTEM 1

typedef uint64_t (*resource_on_open_t)(resource_t*, file_t*);
typedef void (*resource_on_free_t)(void*);

typedef struct resource
{
    node_t node;
    const file_ops_t* ops;
    void* private;
    resource_on_open_t onOpen;
    resource_on_free_t onFree;
    atomic_uint64_t ref;
    atomic_bool hidden;
} resource_t;

void sysfs_init(void);

resource_t* sysfs_expose(const char* path, const char* filename, const file_ops_t* ops, void* private, resource_on_open_t onOpen,
    resource_on_free_t onFree);

void sysfs_hide(resource_t* resource);
