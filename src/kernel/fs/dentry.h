#pragma once

#include "sched/wait.h"
#include "sync/lock.h"
#include "utils/map.h"

#include <stdatomic.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct dentry dentry_t;
typedef struct dentry_ops dentry_ops_t;
typedef struct inode inode_t;
typedef struct superblock superblock_t;

#define DENTRY_DEFER(dentry) __attribute__((cleanup(dentry_defer_cleanup))) dentry_t* CONCAT(i, __COUNTER__) = (dentry)

typedef uint64_t dentry_id_t;

#define DENTRY_IS_ROOT(dentry) (dentry->parent == dentry)

typedef enum
{
    DENTRY_NONE = 0,
    DENTRY_MOUNTPOINT = 1 << 0,
    DENTRY_NEGATIVE = 1 << 1,
    DENTRY_LOOKUP_PENDING = 1 << 2,
} dentry_flags_t;

typedef struct dentry
{
    dentry_id_t id;
    atomic_uint64_t ref;
    char name[MAX_NAME];
    inode_t* inode;
    dentry_t* parent;
    list_entry_t siblingEntry;
    list_t children;
    superblock_t* superblock;
    const dentry_ops_t* ops;
    void* private;
    dentry_flags_t flags;
    lock_t lock;
    wait_queue_t lookupWaitQueue;
    map_entry_t mapEntry;
} dentry_t;

typedef struct dentry_ops
{
    uint64_t (*getdirent)(dentry_t* dentry, dirent_t* buffer, uint64_t amount);
    void (*cleanup)(dentry_t* entry);
} dentry_ops_t;

dentry_t* dentry_new(superblock_t* superblock, dentry_t* parent, const char* name);

void dentry_make_positive(dentry_t* dentry, inode_t* inode);

void dentry_free(dentry_t* dentry);

dentry_t* dentry_ref(dentry_t* dentry);

void dentry_deref(dentry_t* dentry);

static inline void dentry_defer_cleanup(dentry_t** entry)
{
    if (*entry != NULL)
    {
        dentry_deref(*entry);
        *entry = NULL;
    }
}

/**
 * @brief Helper function for a basic getdirent.
 * @ingroup kernel_vfs
 *
 */
uint64_t dentry_generic_getdirent(dentry_t* dentry, dirent_t* buffer, uint64_t amount);
