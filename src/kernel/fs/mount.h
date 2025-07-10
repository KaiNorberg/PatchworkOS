#pragma once

#include "utils/map.h"

#include <stdatomic.h>
#include <stdint.h>

typedef struct mount mount_t;
typedef struct superblock superblock_t;
typedef struct dentry dentry_t;
typedef struct path path_t;

#define MOUNT_DEFER(mount) __attribute__((cleanup(mount_defer_cleanup))) mount_t* CONCAT(i, __COUNTER__) = (mount)

typedef uint64_t mount_id_t;

typedef struct mount
{
    mount_id_t id;
    atomic_uint64_t ref;
    superblock_t* superblock;
    dentry_t* mountpoint;
    mount_t* parent;
    map_entry_t mapEntry;
} mount_t;

mount_t* mount_new(superblock_t* superblock, path_t* mountpoint);

void mount_free(mount_t* mount);

mount_t* mount_ref(mount_t* mount);

void mount_deref(mount_t* mount);

static inline void mount_defer_cleanup(mount_t** mount)
{
    if (*mount != NULL)
    {
        mount_deref(*mount);
    }
}
