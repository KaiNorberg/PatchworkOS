#pragma once

#include "sched/wait.h"
#include "sync/mutex.h"
#include "utils/map.h"
#include "utils/ref.h"

#include <stdatomic.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct dentry dentry_t;
typedef struct dentry_ops dentry_ops_t;
typedef struct inode inode_t;
typedef struct superblock superblock_t;

typedef uint64_t dentry_id_t;

#define DENTRY_IS_ROOT(dentry) (dentry->parent == dentry)

typedef enum
{
    DENTRY_NONE = 0,
    DENTRY_MOUNTPOINT = 1 << 0,
    DENTRY_NEGATIVE = 1 << 1,
} dentry_flags_t;

typedef struct dentry
{
    ref_t ref;
    dentry_id_t id;
    char name[MAX_NAME];
    inode_t* inode;
    dentry_t* parent;
    list_entry_t siblingEntry;
    list_t children;
    superblock_t* superblock;
    const dentry_ops_t* ops;
    void* private;
    dentry_flags_t flags;
    mutex_t mutex;
    map_entry_t mapEntry;
} dentry_t;

/**
 * Dentry operations structure.
 * @ingroup kernel_vfs
 *
 * Note that the dentrys mutex will be acquired by the vfs.
 *
 */
typedef struct dentry_ops
{
    uint64_t (*getdents)(dentry_t* dentry, dirent_t* buffer, uint64_t count, uint64_t* offset);
    bool (*removable)(dentry_t* dir);
    void (*cleanup)(dentry_t* entry);
} dentry_ops_t;

dentry_t* dentry_new(superblock_t* superblock, dentry_t* parent, const char* name);
void dentry_free(dentry_t* dentry);

void dentry_make_positive(dentry_t* dentry, inode_t* inode);

/**
 * @brief Helper function for a basic getdents.
 * @ingroup kernel_vfs
 *
 */
uint64_t dentry_generic_getdents(dentry_t* dentry, dirent_t* buffer, uint64_t count, uint64_t* offset);
