#pragma once

#include <common/node.h>
#include <stdatomic.h>

#include "defs.h"

typedef struct volume volume_t;
typedef struct file file_t;

// Note: Avoid code duplication by using the standard functions when possible

// TODO: Implement namespace system

#define SYSFS_STANDARD_OPEN_DEFINE(name, supportedFlags, ...) \
    static file_t* name(volume_t* volume, const path_t* path, sysobj_t* sysobj) \
    { \
        file_t* file = file_new(volume, path, supportedFlags); \
        if (file == NULL) \
        { \
            return NULL; \
        } \
        static file_ops_t fileOps = __VA_ARGS__; \
        file->ops = &fileOps; \
        file->private = sysobj->private; \
        return file; \
    }

#define SYSFS_STANDARD_OPS_DEFINE(name, supportedFlags, ...) \
    SYSFS_STANDARD_OPEN_DEFINE(name##_standard_open, supportedFlags, __VA_ARGS__) \
    static sysobj_ops_t name = { \
        .open = name##_standard_open, \
    };

#define SYSFS_OBJ 0
#define SYSFS_DIR 1

typedef struct sysobj sysobj_t;
typedef struct sysdir sysdir_t;

typedef file_t* (*sysobj_open_t)(volume_t*, const char*, sysobj_t*);
typedef uint64_t (*sysobj_open2_t)(volume_t*, const char*, sysobj_t*, file_t* [2]);
typedef void (*sysobj_cleanup_t)(sysobj_t*, file_t* file);

typedef void (*sysobj_on_free_t)(sysobj_t*);
typedef void (*sysdir_on_free_t)(sysdir_t*);

typedef struct sysobj_ops
{
    sysobj_open_t open;
    sysobj_open2_t open2;
    sysobj_cleanup_t cleanup;
} sysobj_ops_t;

typedef struct syshdr
{
    node_t node;
    atomic_bool hidden;
    atomic_uint64_t ref;
} syshdr_t;

typedef struct sysobj
{
    syshdr_t header;
    void* private;
    const sysobj_ops_t* ops;
    sysdir_t* dir;
    sysobj_on_free_t onFree;
} sysobj_t;

typedef struct sysdir
{
    syshdr_t header;
    void* private;
    sysdir_on_free_t onFree;
} sysdir_t;

void sysfs_init(void);

// The vfs exposes its volumes in the sysfs, however this creates a circular dependency so we first call sysfs_init()
// then wait to mount sysfs untill after vfs_init().
void sysfs_mount_to_vfs(void);

// Called in the vfs before calling any operation on a sysfs file.
uint64_t sysfs_start_op(file_t* file);

// Called in the vfs after calling any operation on a sysfs file.
void sysfs_end_op(file_t* file);

uint64_t sysdir_init(sysdir_t* dir, const char* path, const char* dirname, void* private);

void sysdir_deinit(sysdir_t* dir, sysdir_on_free_t onFree);

uint64_t sysobj_init(sysobj_t* sysobj, sysdir_t* dir, const char* filename, const sysobj_ops_t* ops, void* private);

uint64_t sysobj_init_path(sysobj_t* sysobj, const char* path, const char* filename, const sysobj_ops_t* ops,
    void* private);

void sysobj_deinit(sysobj_t* sysobj, sysobj_on_free_t onFree);