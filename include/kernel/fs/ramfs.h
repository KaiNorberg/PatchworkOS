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
 * A simple in-memory filesystem thats loaded from the bootloader. All data is lost when the system is powered off or
 * rebooted.
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
 * @brief Dentry private data for ramfs.
 */
typedef struct
{
    list_entry_t entry;
    dentry_t* dentry;
} ramfs_dentry_data_t;

/**
 * @brief Registers the ramfs filesystem and mounts it as the root filesystem.
 *
 * @param disk The boot disk from the bootloader.
 */
void ramfs_init(const boot_disk_t* disk);

/** @} */
