#pragma once

#include <stdatomic.h>
#include <sys/node.h>

#include "defs.h"
#include "vfs.h"

#define SYSFS_RESOURCE 0
#define SYSFS_SYSTEM 1

typedef file_t* (*resource_open_t)(volume_t*, resource_t*);
typedef uint64_t (*resource_open2_t)(volume_t*, resource_t*, file_t* [2]);
typedef void (*resource_on_cleanup_t)(resource_t*, file_t* file);
typedef void (*resource_on_free_t)(resource_t*);

// Avoid code duplication by using the standard functions when possible

#define SYSFS_STANDARD_RESOURCE_OPEN(name, fileOps) \
    static file_t* name(volume_t* volume, resource_t* resource) \
    { \
        file_t* file = file_new(volume); \
        if (file == NULL) \
        { \
            return NULL; \
        } \
        file->ops = fileOps; \
        file->private = resource->private; \
        return file; \
    }

#define SYSFS_STANDARD_RESOURCE_OPS(name, fileOps) \
    SYSFS_STANDARD_RESOURCE_OPEN(name##_standard_open, fileOps) \
    static resource_ops_t name = { \
        .open = name##_standard_open, \
    };

typedef struct resource_ops
{
    resource_open_t open;
    resource_open2_t open2;
    resource_on_cleanup_t onCleanup;
    resource_on_free_t onFree;
} resource_ops_t;

typedef struct resource
{
    node_t node;
    void* private;
    const resource_ops_t* ops;
    atomic_uint64_t ref;
    atomic_bool hidden;
} resource_t;

void sysfs_init(void);

resource_t* sysfs_expose(const char* path, const char* filename, const resource_ops_t* ops, void* private);

void sysfs_hide(resource_t* resource);