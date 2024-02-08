#pragma once

#include "vfs/vfs.h"
#include "list/list.h"

typedef struct
{
    
} Device;

typedef struct
{

} DeviceBus;

void device_disk_init();

Device* device_new(DeviceBus* bus, const char* name);

DeviceBus* device_bus_new(const char* name);