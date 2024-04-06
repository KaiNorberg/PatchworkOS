#pragma once

#include "defs/defs.h"
#include "vfs/vfs.h"
#include "array/array.h"

typedef struct SysNode
{
    char* name;
    Array* children;
} SysNode;

void sysfs_init();