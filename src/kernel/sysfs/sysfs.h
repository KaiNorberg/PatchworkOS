#pragma once

#include "defs/defs.h"
#include "vfs/vfs.h"
#include "array/array.h"

typedef enum
{
    SYS_NODE_TYPE_FILE,
    SYS_NODE_TYPE_DIR
} SysNodeType;

typedef struct
{
    char* name;
    void* internal;
    SysNodeType type;
    Array* children;
    _Atomic(uint64_t) ref;
} SysNode;

void sysfs_init();