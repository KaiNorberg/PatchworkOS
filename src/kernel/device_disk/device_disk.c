#include "device_disk.h"

#include <libc/string.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "vfs/vfs.h"

static List* buses;

Status device_disk_open(Disk* disk, File** out, const char* path, uint64_t flags)
{
    (*out) = file_new(disk, 0, flags);

    return STATUS_SUCCESS;
}

Status device_disk_close(File* file)
{
    kfree(file);

    return STATUS_SUCCESS;
}

Status device_disk_read(File* file, void* buffer, uint64_t length)
{
    memset(buffer, 'A', length);

    return STATUS_SUCCESS;
}

Status device_disk_seek(File* file, int64_t offset, uint64_t origin)
{
    switch (origin)
    {
    case FILE_SEEK_SET:
    {
        file->position = offset;
    }
    break;
    case FILE_SEEK_CUR:
    {
        file->position += offset;
    }
    break;
    case FILE_SEEK_END:
    {
        return STATUS_NOT_ALLOWED;
    }
    break;
    }

    return STATUS_SUCCESS;
}

void device_disk_init()
{    
    tty_start_message("Device disk initializing");

    Disk* disk = disk_new("dev", 0);
    disk->open = device_disk_open;
    disk->close = device_disk_close;
    disk->read = device_disk_read;
    disk->seek = device_disk_seek;
    
    Status status = vfs_mount(disk);
    if (status != STATUS_SUCCESS)
    {
        tty_print(statusToString[status]);
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}

DeviceBus* device_bus_new(const char* name)
{
    DeviceBus* deviceBus = kmalloc(sizeof(DeviceBus));
    memset(deviceBus, 0, sizeof(DeviceBus));

    strcpy(deviceBus->name, name);

    deviceBus->devices = list_new();

    return deviceBus;
}

Device* device_new(DeviceBus* bus, const char* name)
{
    Device* device = kmalloc(sizeof(Device));
    memset(device, 0, sizeof(Device));

    strcpy(device->name, name);

    return device;
}