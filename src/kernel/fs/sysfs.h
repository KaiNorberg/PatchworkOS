#pragma once

#include <common/node.h>
#include <stdatomic.h>

#include "defs.h"

// TODO: Overhaul sysfs. Implement seperate /dev, /proc folders. Currently there is no way to set the inode size and similar.

typedef struct file file_t;
typedef struct file_ops file_ops_t;

#define SYSFS_NAME "sysfs"

#define SYSFS_OBJ 0
#define SYSFS_DIR 1

typedef struct sysobj sysobj_t;
typedef struct sysdir sysdir_t;

typedef void (*sysobj_on_free_t)(sysobj_t*);
typedef void (*sysdir_on_free_t)(sysdir_t*);

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
    const file_ops_t* ops;
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
void syfs_after_vfs_init(void);

// Called in the vfs before calling any operation on a sysfs file.
uint64_t sysfs_start_op(file_t* file);

// Called in the vfs after calling any operation on a sysfs file.
void sysfs_end_op(file_t* file);

uint64_t sysdir_init(sysdir_t* dir, const char* path, const char* dirname, void* private);

void sysdir_deinit(sysdir_t* dir, sysdir_on_free_t onFree);

uint64_t sysobj_init(sysobj_t* sysobj, sysdir_t* dir, const char* filename, const file_ops_t* ops, void* private);

uint64_t sysobj_init_path(sysobj_t* sysobj, const char* path, const char* filename, const file_ops_t* ops,
    void* private);

void sysobj_deinit(sysobj_t* sysobj, sysobj_on_free_t onFree);