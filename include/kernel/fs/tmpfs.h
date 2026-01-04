#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/superblock.h>

#include <boot/boot_info.h>

#include <sys/io.h>
#include <sys/list.h>

/**
 * @brief Temporary Filesystem.
 * @defgroup kernel_fs_tmpfs Temporary Filesystem
 * @ingroup kernel_fs
 *
 * A simple in-memory filesystem. All data is lost when power is lost.
 *
 * @{
 */

/**
 * @brief The name of the tmpfs filesystem.
 */
#define TMPFS_NAME "tmpfs"

/**
 * @brief Superblock private data for tmpfs.
 */
typedef struct
{
    list_t dentrys; // We store all dentries in here to keep them in memory.
    lock_t lock;
} tmpfs_superblock_data_t;

/**
 * @brief Registers the tmpfs filesystem and mounts an instance of it containing the boot ram disk as root.
 */
void tmpfs_init(void);

/** @} */
