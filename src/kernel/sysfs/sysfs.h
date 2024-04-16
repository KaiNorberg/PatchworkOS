#pragma once

#include "defs/defs.h"
#include "list/list.h"
#include "vfs/vfs.h"

/*typedef struct Object Object;

typedef enum
{
    NODE_TYPE_STORE,
    NODE_TYPE_RESOURCE
} NodeType;

typedef struct Node
{
    ListEntry base;
    char name[CONFIG_MAX_NAME];
    NodeType type;
} Node;

typedef struct
{
    Node base;
    List objects;
    Lock lock;
} Namespace;

typedef struct
{
    Node base;
    void (*cleanup)(Object*);
    uint64_t (*read)(Object*, void*, uint64_t);
    uint64_t (*write)(Object*, const void*, uint64_t);
    uint64_t (*seek)(Object*, int64_t, uint8_t);
} Object;*/

void sysfs_init();