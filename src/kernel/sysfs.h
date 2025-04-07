#pragma once

#include <stdatomic.h>
#include <sys/node.h>

#include "defs.h"
#include "vfs.h"

#define SYSFS_RESOURCE 0
#define SYSFS_SYSTEM 1

typedef file_t* (*resource_open_t)(volume_t*, resource_t*);
typedef uint64_t (*resource_open2_t)(volume_t*, resource_t*, file_t* [2]);
typedef void (*resource_on_free_t)(resource_t*);

// If a resource might need to be freed then this must be called in all file cleanup functions.
// Also note that it frees the resource struct if its reference count reaches 0.
// Returns true if the resource has been freed, false otherwise.
#define SYSFS_CLEANUP(file) \
    ({ \
        uint64_t ref = atomic_fetch_sub(&(file)->resource->ref, 1); \
        bool result = ref <= 1; \
        if (result) \
        { \
            if ((file)->resource->ops->onFree != NULL) \
            { \
                (file)->resource->ops->onFree((file)->resource); \
            } \
            free((file)->resource); \
        } \
        result; \
    })

typedef struct resource_ops
{
    resource_open_t open;
    resource_open2_t open2;
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
