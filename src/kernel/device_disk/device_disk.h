#pragma once

#include "vfs/vfs.h"
#include "list/list.h"

typedef struct
{
    char name[VFS_MAX_NAME_LENGTH];
    Status (*read)(uint64_t position, void* buffer, uint64_t length);
    Status (*write)(uint64_t position, const void* buffer, uint64_t length);
} Device;

typedef struct
{
    char name[VFS_MAX_NAME_LENGTH];
    List* devices;
} DeviceBus;

void device_disk_init();

DeviceBus* device_bus_new(const char* name);

Device* device_new(DeviceBus* bus, const char* name);