#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/vnode.h>

#include <boot/boot_info.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/pmm.h>

#include <sys/fs.h>
#include <sys/list.h>

/**
 * @brief Temporary Filesystem.
 * @defgroup kernel_fs_tmpfs Temporary Filesystem
 * @ingroup kernel_fs
 *
 * A simple in-memory filesystem. All data is lost when power is lost.
 *
 * In addition to tmpfs we also have ramfs which represents the ram disk passed from the bootloader. Ramfs can only be
 * cloned once, after the first clone the ramfs filesystem will be unregistered. Tmpfs can be cloned any number of
 * times.
 * @{
 */

/**
 * @brief The name of the tmpfs filesystem.
 */
#define TMPFS_NAME "tmpfs"

/**
 * @brief Volume data for tmpfs.
 * @struct tmpfs_volume_t
 */
typedef struct
{
    ref_t ref;
    file_volume_t id;
    vnode_t* rootVnode;
    list_t dentries; // We store all dentries in here to keep them in memory.
    lock_t lock;
} tmpfs_volume_t;

/**
 * @brief Vnode data for tmpfs.
 * @struct tmpfs_vnode_t
 */
typedef struct
{
    vnode_t vnode;
    pagevec_t pages;
    size_t size;
    tmpfs_volume_t* volume;
    time_t atime;
    time_t mtime;
    time_t ctime;
    time_t btime;
    uint64_t nlink;
} tmpfs_vnode_t;

/**
 * @brief Registers the tmpfs filesystem and the ramfs filesystem for the bootloaders ram disk.
 */
void tmpfs_init(void);

/** @} */
