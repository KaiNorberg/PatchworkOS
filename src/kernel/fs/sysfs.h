#pragma once

#include "dentry.h"
#include "inode.h"

/**
 * @brief Filesystem for exposing kernel resources.
 * @ingroup kernel_fs
 * @defgroup kernel_fs_sysfs SysFS
 *
 *
 */

// TODO: Implement dev, net and proc groups.

typedef struct file file_t;
typedef struct file_ops file_ops_t;

#define SYSFS_NAME "sysfs"

typedef struct sysfs_file
{
    dentry_t* dentry;
} sysfs_file_t;

typedef struct sysfs_dir
{
    dentry_t* dentry;
} sysfs_dir_t;

typedef struct sysfs_group
{
    sysfs_dir_t root;
    pathname_t mountpoint;
} sysfs_group_t;

void sysfs_init(void);

sysfs_dir_t* sysfs_get_default(void);

uint64_t sysfs_group_init(sysfs_group_t* group, const pathname_t* mountpoint);
uint64_t sysfs_group_deinit(sysfs_group_t* group);

uint64_t sysfs_dir_init(sysfs_dir_t* dir, sysfs_dir_t* parent, const char* name, const inode_ops_t* inodeOps,
    void* private);
void sysfs_dir_deinit(sysfs_dir_t* dir);

uint64_t sysfs_file_init(sysfs_file_t* file, sysfs_dir_t* parent, const char* name, const inode_ops_t* inodeOps,
    const file_ops_t* fileOps, void* private);
void sysfs_file_deinit(sysfs_file_t* file);
