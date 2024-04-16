#pragma once

#include <common/boot_info/boot_info.h>

#include "vfs/vfs.h"
#include "vfs/volume/volume.h"
#include "vfs/file/file.h"

typedef struct
{
    Volume base;
} RamfsVolume;

typedef struct
{
    File base;
    RamFile* ramFile;
} RamfsFile;

void ramfs_init(RamDir* ramRoot);