#pragma once

#include <common/node.h>
#include <stdatomic.h>

#include "defs.h"
#include "dentry.h"

typedef struct file file_t;
typedef struct file_ops file_ops_t;

#define SYSFS_NAME "sysfs"

typedef struct sysfs_file sysfs_file_t;
typedef struct sysfs_dir sysfs_dir_t;

typedef void (*sysfs_file_on_free_t)(sysfs_file_t*);
typedef void (*sysfs_dir_on_free_t)(sysfs_dir_t*);

typedef struct sysfs_file
{
    dentry_t* dentry;
    void* private;
    sysfs_file_on_free_t onFree;
} sysfs_file_t;

typedef struct sysfs_dir
{
    dentry_t* dentry;
    void* private;
    sysfs_dir_on_free_t onFree;
} sysfs_dir_t;

typedef struct sysfs_group
{
    sysfs_dir_t root;
    char mountpoint[MAX_PATH];
} sysfs_group_t;

void sysfs_init(void);

// The vfs may be exposing its structues using sysfs, this creates a circular dependency so we first call sysfs_init()
// then wait to mount sysfs untill after vfs_init().
void syfs_after_vfs_init(void);

sysfs_group_t* sysfs_default_ns(void);

uint64_t sysfs_group_init(sysfs_group_t* ns);
void sysfs_group_deinit(sysfs_group_t* ns);
uint64_t sysfs_group_mount(sysfs_group_t* ns, const char* mountpoint);
uint64_t sysfs_group_unmount(sysfs_group_t* ns);

uint64_t sysfs_dir_init(sysfs_dir_t* sysfs_dir, const char* name, void* private);
void sysfs_dir_deinit(sysfs_dir_t* sysfs_dir, sysfs_dir_on_free_t callback);
uint64_t sysfs_dir_add_dir(sysfs_dir_t* parent, sysfs_dir_t* child);
uint64_t sysfs_dir_add_file(sysfs_dir_t* parent, sysfs_file_t* sysfs_file);
void sysfs_dir_remove_dir(sysfs_dir_t* parent, sysfs_dir_t* child);
void sysfs_dir_remove_file(sysfs_dir_t* parent, sysfs_file_t* sysfs_file);

uint64_t sysfs_file_init(sysfs_file_t* sysfs_file, const char* name, const file_ops_t* ops, void* private);
void sysfs_file_deinit(sysfs_file_t* sysfs_file,  sysfs_dir_on_free_t callback);