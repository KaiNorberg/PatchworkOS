#pragma once

#include <stdatomic.h>
#include <sys/node.h>

#include "defs.h"
#include "sys/list.h"
#include "vfs.h"

// Avoid code duplication by using the standard functions when possible

// TODO: Implement namespace system

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

#define SYSFS_RESOURCE 0
#define SYSFS_DIR 1

typedef struct resource resource_t;
typedef struct sysdir sysdir_t;

typedef file_t* (*resource_open_t)(volume_t*, resource_t*);
typedef uint64_t (*resource_open2_t)(volume_t*, resource_t*, file_t* [2]);
typedef void (*resource_cleanup_t)(resource_t*, file_t* file);
typedef void (*resource_on_free_t)(resource_t*);

typedef void (*sysdir_on_free_t)(sysdir_t*);

typedef struct resource_ops
{
    resource_open_t open;
    resource_open2_t open2;
    resource_cleanup_t cleanup;
    resource_on_free_t onFree;
} resource_ops_t;

typedef struct resource
{
    node_t node;
    void* private;
    const resource_ops_t* ops;
    atomic_uint64 ref;
    atomic_bool hidden;
    sysdir_t* dir;
} resource_t;

typedef struct sysdir
{
    node_t node;
    void* private;
    sysdir_on_free_t onFree;
    atomic_uint64 ref;
} sysdir_t;

void sysfs_init(void);

// The vfs exposes its volumes in the sysfs, however this creates a circular dependency so we first call sysfs_init() then wait to
// mount sysfs untill after vfs_init().
void sysfs_mount_to_vfs(void);

sysdir_t* sysdir_new(const char* path, const char* dirname, sysdir_on_free_t onFree, void* private);

void sysdir_free(sysdir_t* dir);

uint64_t sysdir_add(sysdir_t* dir, const char* filename, const resource_ops_t* ops, void* private);

resource_t* resource_new(const char* path, const char* filename, const resource_ops_t* ops, void* private);

void resource_free(resource_t* resource);
