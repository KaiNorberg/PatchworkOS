#pragma once

#include "path.h"
#include "sync/mutex.h"
#include "utils/map.h"
#include "utils/ref.h"

#include <stdatomic.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <time.h>

// TODO: Implement actually writing/syncing dirty inodes, for now inodes should use the notify functions but they will
// never actually be "cleaned."

typedef struct inode inode_t;
typedef struct inode_ops inode_ops_t;
typedef struct superblock superblock_t;
typedef struct file_ops file_ops_t;
typedef struct dentry dentry_t;

typedef enum
{
    INODE_NONE = 0, ///< None
} inode_flags_t;

typedef struct inode
{
    ref_t ref;
    inode_number_t number; ///< Constant after creation.
    inode_type_t type;     ///< Constant after creation.
    inode_flags_t flags;
    uint64_t linkCount;
    uint64_t size;
    uint64_t blocks;
    time_t accessTime; ///< Unix time stamp for the last inode access.
    time_t modifyTime; ///< Unix time stamp for last file content alteration.
    time_t changeTime; ///< Unix time stamp for the last file metadata alteration.
    time_t createTime; ///< Unix time stamp for the inode creation.
    void* private;
    superblock_t* superblock;  ///< Constant after creation.
    const inode_ops_t* ops;    ///< Constant after creation.
    const file_ops_t* fileOps; ///< Constant after creation.
    mutex_t mutex;
    map_entry_t mapEntry; ///< Protected by the inodeCache lock.
} inode_t;

/**
 * Inode operations structure.
 * @ingroup kernel_vfs
 *
 * Note that the inodes mutex will be acquired by the vfs.
 *
 */
typedef struct inode_ops
{
    /**
     * @brief Should set the target dentry to be positive (give it an inode), if the entry does not exist the operation
     * should still return success and leave the dentry as negative, if other errors occur it should return ERR and set
     * errno.
     */
    uint64_t (*lookup)(inode_t* dir, dentry_t* target);
    /**
     * @brief Handles both directories and files, works the same as lookup.
     */
    uint64_t (*create)(inode_t* dir, dentry_t* target, path_flags_t flags);
    void (*truncate)(inode_t* target);
    uint64_t (*link)(dentry_t* old, inode_t* dir, dentry_t* target);
    /**
     * @brief Handles both directories and files.
     */
    uint64_t (*delete)(inode_t* parent, dentry_t* target, path_flags_t flags);
    void (*cleanup)(inode_t* inode);
} inode_ops_t;

inode_t* inode_new(superblock_t* superblock, inode_number_t number, inode_type_t type, const inode_ops_t* ops,
    const file_ops_t* fileOps);
void inode_free(inode_t* inode);

void inode_notify_access(inode_t* inode);

void inode_notify_modify(inode_t* inode);

void inode_notify_change(inode_t* inode);

void inode_truncate(inode_t* inode);
