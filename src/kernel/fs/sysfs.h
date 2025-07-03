#pragma once

#include <common/node.h>
#include <stdatomic.h>

#include "defs.h"

// TODO: Overhaul sysfs. Implement seperate /dev, /proc folders. Currently there is no way to set the inode size and
// similar.

typedef struct file file_t;
typedef struct file_ops file_ops_t;

#define SYSFS_NAME "sysfs"

#define SYSFS_OBJ 0
#define SYSFS_DIR 1

typedef struct sysfile sysfile_t;
typedef struct sysdir sysdir_t;

typedef void (*sysfile_on_free_t)(sysfile_t*);
typedef void (*sysdir_on_free_t)(sysdir_t*);

typedef struct sysfile
{
    dentry_t* dentry;
    void* private;
    sysfile_on_free_t onFree;
} sysfile_t;

typedef struct sysdir
{
    dentry_t* dentry;
    void* private;
    sysdir_on_free_t onFree;
} sysdir_t;

typedef struct sysfs_namespace
{
    sysdir_t root;
    char name[MAX_NAME];
} sysfs_namespace_t;

void sysfs_init(void);

// The vfs may be exposing its structues using sysfs, this creates a circular dependency so we first call sysfs_init()
// then wait to mount sysfs untill after vfs_init().
void syfs_after_vfs_init(void);

uint64_t sysfs_namespace_init(sysfs_namespace_t* namespace, const char* name);
void sysfs_namespace_deinit(sysfs_namespace_t* namespace);
uint64_t sysfs_namespace_mount(sysfs_namespace_t* namespace, const char* parent);
void sysfs_namespace_unmount(sysfs_namespace_t* namespace);

uint64_t sysdir_init(sysdir_t* sysdir, const char* name, void* private);
void sysdir_deinit(sysdir_t* sysdir, sysdir_on_free_t callback);
uint64_t sysdir_add_dir(sysdir_t* parent, sysdir_t* child);
uint64_t sysdir_add_file(sysdir_t* parent, sysfile_t* sysfile);
void sysdir_remove_dir(sysdir_t* parent, sysdir_t* child);
void sysdir_remove_file(sysdir_t* parent, sysfile_t* sysfile);

uint64_t sysfile_init(sysfile_t* sysfile, const char* name, const file_ops_t* ops, void* private);
void sysfile_deinit(sysfile_t* sysfile,  sysdir_on_free_t callback);