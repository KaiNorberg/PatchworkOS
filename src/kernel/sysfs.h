#pragma once

#include <stdatomic.h>
#include <sys/node.h>

#include "defs.h"
#include "sys/list.h"
#include "vfs.h"

// Note: Avoid code duplication by using the standard functions when possible

// TODO: Implement namespace system

#define SYSFS_STANDARD_SYSOBJ_OPEN_DEFINE(name, ...) \
    static file_t* name(volume_t* volume, sysobj_t* sysobj) \
    { \
        file_t* file = file_new(volume); \
        if (file == NULL) \
        { \
            return NULL; \
        } \
        static file_ops_t fileOps = __VA_ARGS__; \
        file->ops = &fileOps; \
        file->private = sysobj->private; \
        return file; \
    }

#define SYSFS_STANDARD_SYSOBJ_OPS_DEFINE(name, ...) \
    SYSFS_STANDARD_SYSOBJ_OPEN_DEFINE(name##_standard_open, __VA_ARGS__) \
    static sysobj_ops_t name = { \
        .open = name##_standard_open, \
    };

#define SYSFS_OBJ 0
#define SYSFS_DIR 1

typedef struct sysobj sysobj_t;
typedef struct sysdir sysdir_t;

typedef file_t* (*sysobj_open_t)(volume_t*, sysobj_t*);
typedef uint64_t (*sysobj_open2_t)(volume_t*, sysobj_t*, file_t* [2]);
typedef void (*sysobj_cleanup_t)(sysobj_t*, file_t* file);
typedef void (*sysobj_on_free_t)(sysobj_t*);

typedef void (*sysdir_on_free_t)(sysdir_t*);

typedef struct sysobj_ops
{
    sysobj_open_t open;
    sysobj_open2_t open2;
    sysobj_cleanup_t cleanup;
    sysobj_on_free_t onFree;
} sysobj_ops_t;

typedef struct sysobj
{
    node_t node;
    void* private;
    const sysobj_ops_t* ops;
    atomic_uint64 ref;
    atomic_bool hidden;
    sysdir_t* dir;
} sysobj_t;

typedef struct sysdir
{
    node_t node;
    void* private;
    sysdir_on_free_t onFree;
    atomic_uint64 ref;
} sysdir_t;

void sysfs_init(void);

// The vfs exposes its volumes in the sysfs, however this creates a circular dependency so we first call sysfs_init()
// then wait to mount sysfs untill after vfs_init().
void sysfs_mount_to_vfs(void);

sysdir_t* sysdir_new(const char* path, const char* dirname, sysdir_on_free_t onFree, void* private);

void sysdir_free(sysdir_t* dir);

uint64_t sysdir_add(sysdir_t* dir, const char* filename, const sysobj_ops_t* ops, void* private);

sysobj_t* sysobj_new(const char* path, const char* filename, const sysobj_ops_t* ops, void* private);

void sysobj_free(sysobj_t* sysobj);
