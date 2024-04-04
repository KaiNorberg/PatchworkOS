#pragma once

#include "defs/defs.h"
#include "vfs/vfs.h"
#include "array/array.h"

/*typedef struct
{
    char name[CONFIG_MAX_NAME];
    Array* devices;
} DeviceBus;

typedef struct
{
    char name[CONFIG_MAX_NAME];
    void* context;
} Device;*/

void devfs_init();