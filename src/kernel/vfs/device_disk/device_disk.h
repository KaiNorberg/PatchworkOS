#pragma once

#include "vfs/vfs.h"
#include "list/list.h"

#include "vfs/generic_disk/generic_disk.h"

typedef struct
{
    
} Device;

typedef struct
{
    DirectoryNode* dirNode;
} DeviceBus;

void device_disk_init();

Device* device_new(DeviceBus* bus, const char* name);

DeviceBus* device_bus_new(const char* name);