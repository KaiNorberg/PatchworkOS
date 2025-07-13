#pragma once

#include "utils/map.h"
#include "utils/ref.h"

#include <stdatomic.h>
#include <stdint.h>

typedef struct mount mount_t;
typedef struct superblock superblock_t;
typedef struct dentry dentry_t;
typedef struct path path_t;

typedef uint64_t mount_id_t;

typedef struct mount
{
    ref_t ref;
    mount_id_t id;
    superblock_t* superblock;
    dentry_t* mountpoint;
    mount_t* parent;
    map_entry_t mapEntry;
} mount_t;

mount_t* mount_new(superblock_t* superblock, path_t* mountpoint);
void mount_free(mount_t* mount);
