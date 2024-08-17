#pragma once

#include <stdatomic.h>
#include <sys/node.h>

#include "defs.h"
#include "vfs.h"

#define SYSFS_RESOURCE 0
#define SYSFS_SYSTEM 1

typedef uint64_t (*resource_open_t)(resource_t*, file_t*);
typedef void (*resource_delete_t)(void*);

typedef struct resource
{
    node_t node;
    const file_ops_t* ops;
    void* private;
    resource_open_t open;
    resource_delete_t delete;
    atomic_uint64_t ref;
    atomic_bool hidden;
} resource_t;

void sysfs_init(void);

resource_t* sysfs_expose(const char* path, const char* filename, const file_ops_t* ops, void* private, resource_open_t open,
    resource_delete_t delete);

uint64_t sysfs_hide(resource_t* resource);
