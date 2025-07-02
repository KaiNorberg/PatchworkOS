#pragma once

#include "defs.h"
#include "sync/lock.h"
#include "sync/rwlock.h"
#include "sysfs.h"
#include "utils/map.h"

#include "dentry.h"
#include "file.h"
#include "inode.h"
#include "mount.h"
#include "path.h"
#include "superblock.h"

#include <ctype.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>

// TODO: Implement improved caching, LRU. Let the map_t handle static buffer + wrapper?
// TODO: Implement per-process namespaces.
// TODO: Implement literally everything else.

/**
 * @brief Virtual File System.
 * @ingroup kernel
 * @defgroup kernel_vfs
 *
 */

#define VFS_ROOT_ENTRY_NAME "__root__"

#define VFS_DEVICE_NAME_NONE "__none__"

typedef struct filesystem filesystem_t;

typedef struct filesystem
{
    list_entry_t entry;
    const char* name;
    superblock_t* (*mount)(filesystem_t* fs, const char* deviceName, superblock_flags_t flags, const void* private);
} filesystem_t;

typedef struct
{
    list_t list;
    rwlock_t lock;
} vfs_list_t;

typedef struct
{
    map_t map;
    rwlock_t lock;
} vfs_map_t;

typedef struct
{
    mount_t* mount;
    rwlock_t lock;
} vfs_root_t;

void vfs_init(void);

uint64_t vfs_get_new_id(void);

uint64_t vfs_register_fs(filesystem_t* fs);
uint64_t vfs_unregister_fs(filesystem_t* fs);
filesystem_t* vfs_get_fs(const char* name);

uint64_t vfs_get_global_root(path_t* outRoot);
uint64_t vfs_mountpoint_to_mount_root(path_t* outRoot, const path_t* mountpoint);

inode_t* vfs_get_inode(superblock_t* superblock, inode_number_t number);
dentry_t* vfs_get_dentry(dentry_t* parent, const char* name);

void vfs_remove_superblock(superblock_t* superblock);
void vfs_remove_inode(inode_t* inode);
void vfs_remove_dentry(dentry_t* dentry);

uint64_t vfs_lookup(path_t* outPath, const char* pathname);
uint64_t vfs_lookup_parent(path_t* outPath, const char* pathname, char* outLastName);

uint64_t vfs_mount(const char* deviceName, const char* mountpoint, const char* fsName, superblock_flags_t flags,
    const void* private);
uint64_t vfs_unmount(const char* mountpoint);

bool vfs_is_name_valid(const char* name);

/**
 * @brief Fills a `stat_t` struct.
 * @ingroup kernel_vfs
 *
 * @param buffer The destination buffer.
 * @param inode The inode whose information will fill the buffer. This function does NOT acquire the inodes lock.
 * @param flags
 * @param name
 */
uint64_t vfs_fill_stat_buffer(stat_t* buffer, inode_t* inode, dentry_flags_t flags, const char* name);

file_t* vfs_open(const char* pathname);
uint64_t vfs_open2(const char* pathname, file_t* files[2]);
uint64_t vfs_read(file_t* file, void* buffer, uint64_t count);
uint64_t vfs_write(file_t* file, const void* buffer, uint64_t count);
uint64_t vfs_seek(file_t* file, int64_t offset, seek_origin_t origin);
uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size);
void* vfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot);
uint64_t vfs_poll(poll_file_t* files, uint64_t amount, clock_t timeout);

uint64_t vfs_getdirent(file_t* file, dirent_t* buffer, uint64_t amount);
uint64_t vfs_stat(const char* pathname, stat_t* buffer);
uint64_t vfs_link(const char* oldPathname, const char* newPathname);
uint64_t vfs_rename(const char* oldPathname, const char* newPathname);
uint64_t vfs_remove(const char* pathname);

/**
 * @brief Helper struct for implemeting 'getdirent()'.
 * @ingroup kernel_vfs
 *
 */
typedef struct
{
    uint64_t index; //!< The current index into the destination buffer.
    uint64_t total; //!< The total number of entries written to the destination buffer, is also the result of the
                    //! `getdirent()` function.
} getdirent_ctx_t;

/**
 * @brief Helper function for implemeting 'getdirent()'.
 * @ingroup kernel_vfs
 *
 * @param ctx The getdirent context, should be zero initialized.
 * @param buffer The destination buffer.
 * @param amount The amount of `dirent_t` that will fitt in `buffer`.
 * @param number The inode number to be written to the `buffer`.
 * @param type The inode type to be written to the `buffer`.
 * @param name The name to be written to the `buffer`.
 */
void getdirent_write(getdirent_ctx_t* ctx, dirent_t* buffer, uint64_t amount, inode_number_t number, inode_type_t type,
    const char* name);

// Helper macros for implementing file operations dealing with simple buffers
#define BUFFER_READ(buffer, count, offset, src, size) \
    ({ \
        uint64_t readCount = (*(offset) <= (size)) ? MIN((count), (size) - *(offset)) : 0; \
        memcpy((buffer), (src) + *(offset), readCount); \
        *(offset) += readCount; \
        readCount; \
    })
