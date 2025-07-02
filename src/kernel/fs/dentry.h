#pragma once

#include "sync/lock.h"
#include "utils/map.h"

#include <stdatomic.h>
#include <stdint.h>
#include <sys/io.h>

typedef struct dentry dentry_t;
typedef struct dentry_ops dentry_ops_t;
typedef struct inode inode_t;
typedef struct superblock superblock_t;

#define DENTRY_DEFER(entry) __attribute__((cleanup(dentry_defer_cleanup))) dentry_t* CONCAT(i, __COUNTER__) = (entry)

typedef uint64_t dentry_id_t;

typedef struct dentry
{
    map_entry_t mapEntry;
    dentry_id_t id;
    atomic_uint64_t ref;
    char name[MAX_NAME];
    inode_t* inode;
    dentry_t* parent;
    superblock_t* superblock;
    const dentry_ops_t* ops;
    void* private;
    dentry_flags_t flags; // Protected by ::lock
    lock_t lock;
} dentry_t;

typedef struct dentry_ops
{
    void (*cleanup)(dentry_t* entry);
} dentry_ops_t;

dentry_t* dentry_new(superblock_t* superblock, const char* name, inode_t* inode);

void dentry_free(dentry_t* dentry);

dentry_t* dentry_ref(dentry_t* dentry);

void dentry_deref(dentry_t* dentry);

static inline void dentry_defer_cleanup(dentry_t** entry)
{
    if (*entry != NULL)
    {
        dentry_deref(*entry);
    }
}