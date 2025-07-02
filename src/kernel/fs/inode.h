#pragma once

#include "path.h"
#include "sync/lock.h"
#include "utils/map.h"

#include <stdatomic.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <time.h>

// TODO: Implement actually writing/syncing dirty inodes, for now inodes should be marked as dirty as appropriate but
// they will never actually be "cleaned."

typedef struct inode inode_t;
typedef struct inode_ops inode_ops_t;
typedef struct superblock superblock_t;
typedef struct file_ops file_ops_t;
typedef struct dentry dentry_t;

#define INODE_DEFER(inode) __attribute__((cleanup(inode_defer_cleanup))) inode_t* CONCAT(i, __COUNTER__) = (inode)

typedef enum
{
    INODE_NONE = 0,
    INODE_DIRTY = 1 << 0,
} inode_flags_t;

typedef struct inode
{
    map_entry_t mapEntry;  //!< Protected by the inodeCache lock.
    inode_number_t number; //!< Constant after creation.
    atomic_uint64_t ref;
    inode_type_t type; //!< Constant after creation.
    inode_flags_t flags;
    uint64_t linkCount;
    uint64_t size;
    uint64_t blocks;
    time_t accessTime; //!< Unix time stamp for the last inode access.
    time_t modifyTime; //!< Unix time stamp for last file content alteration.
    time_t changeTime; //!< Unix time stamp for the last file metadata alternation.
    void* private;
    superblock_t* superblock;  //!< Constant after creation.
    const inode_ops_t* ops;    //!< Constant after creation.
    const file_ops_t* fileOps; //!< Constant after creation.
    lock_t
        lock; //!< Lock for the mutable members of the inode, also used to sync the position of files inside the inode.
} inode_t;

typedef struct inode_ops
{
    dentry_t* (*lookup)(inode_t* parent, const char* name);
    inode_t* (*create)(inode_t* parent, const char* name, path_flags_t flags);
    void (*truncate)(inode_t* inode);
} inode_ops_t;

inode_t* inode_new(superblock_t* superblock, inode_number_t number, inode_type_t type, inode_ops_t* ops,
    file_ops_t* fileOps);

void inode_free(inode_t* inode);

inode_t* inode_ref(inode_t* inode);

void inode_deref(inode_t* inode);

uint64_t inode_sync(inode_t* inode);

static inline void inode_defer_cleanup(inode_t** inode)
{
    if (*inode != NULL)
    {
        inode_deref(*inode);
    }
}