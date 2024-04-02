#include "devfs.h"

#include <string.h>

#include "tty/tty.h"
#include "vfs/utils/utils.h"

static Filesystem devfs;
static Array* buses;

/*static FindResult device_bus_find_callback(void* element, void* context)
{
    DeviceBus* bus = element;
    const char* name = context;

    return vfs_compare_names(bus->name, name) ? FIND_FOUND : FIND_NOT_FOUND;
}

static inline Device* device_bus_find(const char* name)
{    
    const char* busName = vfs_first_dir(path);
    if (busName == NULL)
    {
        return NULL;
    }
    
    DeviceBus* bus = array_find(buses, device_bus_find_callback, (char*)busName);
    if (bus == NULL)
    {
        return NULL;
    }

    
}

static inline Device* device_find(const char* path)
{    
    const char* busName = vfs_first_dir(path);
    if (busName == NULL)
    {
        return NULL;
    }
    
    DeviceBus* bus = array_find(buses, device_bus_find_callback, (char*)busName);
    if (bus == NULL)
    {
        return NULL;
    }


}

uint64_t devfs_open(Disk* disk, File* file, const char* path)
{
    const char* busName = vfs_first_dir(path);
    if ()

    DeviceBus* bus = device_bus_find(busName);
    if (bus)


    return 0;
}*/

void devfs_init()
{
    tty_start_message("Devfs initializing");

    buses = array_new();

    memset(&devfs, 0, sizeof(Filesystem));
    strcpy(devfs.name, "devfs");

    if (vfs_mount("dev", NULL, &devfs) == ERR)
    {
        tty_print("Failed to mount devfs");
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}