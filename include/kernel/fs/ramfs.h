#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/superblock.h>

#include <boot/boot_info.h>

#include <sys/io.h>
#include <sys/list.h>

/**
 * @brief RAM filesystem.
 * @defgroup kernel_fs_ramfs RAMFS
 * @ingroup kernel_fs
 *
 * A simple in-memory filesystem. All data is lost when power is lost.
 *
 * @{
 */

/**
 * @brief The name of the ramfs filesystem.
 */
#define RAMFS_NAME "ramfs"

/**
 * @brief Superblock private data for ramfs.
 */
typedef struct
{
    list_t dentrys; // We store all dentries in here to keep them in memory.
    lock_t lock;
} ramfs_superblock_data_t;

/**
 * @brief Registers the ramfs filesystem and mounts an instance of it containing the boot ram disk as root.
 */
void ramfs_init(void);

/** @} */
