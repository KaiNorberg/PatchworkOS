#include "device_disk.h"

#include "tty/tty.h"
#include "heap/heap.h"

#include "vfs/vfs.h"
#include "vfs/utils/utils.h"

#include <libc/string.h>

void device_disk_init()
{
    /*DirectoryNode* ps2 = generic_disk_create_dir(disk->root, "ps2");
    generic_disk_create_file(ps2, "keyboard");*/

    /*Status status = vfs_mount(disk->disk, "dev");
    if (status != STATUS_SUCCESS)
    {
        tty_print("DEVICE DISK: ");
        tty_print(statusToString[status]);
        tty_end_message(TTY_MESSAGE_ER);
    }*/

    /*File* file;
    status = vfs_open(&file, "dev:/ps2/keyboard", 0);
    if (status != STATUS_SUCCESS)
    {
        tty_print("DEVICE DISK: ");
        tty_print(statusToString[status]);
        tty_end_message(TTY_MESSAGE_ER);
    }*/
}

/*Device* device_new(DeviceBus* device, const char* name)
{
    Device* device = kmalloc(sizeof(Device));
    memset(device, 0, sizeof(Device));

    strcpy(device->name, name);

    return device;
}

DeviceBus* device_bus_new(const char* name)
{
    DeviceBus* deviceBus = kmalloc(sizeof(DeviceBus));
    memset(deviceBus, 0, sizeof(DeviceBus));

    strcpy(deviceBus->name, name);

    deviceBus->devices = list_new();
    deviceBus->children = list_new();

    return deviceBus;
}*/