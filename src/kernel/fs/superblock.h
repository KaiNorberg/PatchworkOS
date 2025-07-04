#pragma once

#include "sysfs.h"

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct filesystem filesystem_t;
typedef struct superblock superblock_t;
typedef struct superblock_ops superblock_ops_t;
typedef struct dentry_ops dentry_ops_t;
typedef struct inode inode_t;
typedef struct dentry dentry_t;

#define SUPER_DEFER(superblock) \
    __attribute__((cleanup(superblock_defer_cleanup))) superblock_t* CONCAT(i, __COUNTER__) = (superblock)

typedef uint64_t superblock_id_t;

typedef enum
{
    SUPER_NONE = 0,
} superblock_flags_t;

typedef struct superblock
{
    list_entry_t entry;
    superblock_id_t id;
    atomic_uint64_t ref;
    uint64_t blockSize;
    uint64_t maxFileSize;
    superblock_flags_t flags;
    void* private;
    dentry_t* root;
    const superblock_ops_t* ops;
    const dentry_ops_t* dentryOps;
    filesystem_t* fs;
    char deviceName[MAX_NAME];
    sysfs_dir_t sysfs_dir;
} superblock_t;

typedef struct superblock_ops
{
    inode_t* (*allocInode)(superblock_t* superblock);
    void (*freeInode)(superblock_t* superblock, inode_t* inode);
    uint64_t (*writeInode)(superblock_t* superblock, inode_t* inode);
    void (*cleanup)(superblock_t* superblock);
} superblock_ops_t;

superblock_t* superblock_new(filesystem_t* fs, const char* deviceName, superblock_ops_t* ops,
    dentry_ops_t* dentryOps);
void superblock_free(superblock_t* superblock);
superblock_t* superblock_ref(superblock_t* superblock);
void superblock_deref(superblock_t* superblock);

static inline void superblock_defer_cleanup(superblock_t** superblock)
{
    if (*superblock != NULL)
    {
        superblock_deref(*superblock);
    }
}