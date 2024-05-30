#include "net.h"

#include <errno.h>

#include "sched.h"
#include "tty.h"

/*static uint64_t server_local_read(File* file, void* buffer, uint64_t count)
{

}*/

static File* net_annouce_local(const char* address)
{
    return NULLPTR(EIMPL);
}

File* net_announce(const char* address)
{
    const char* domain = address;

    address = strchr(address, VFS_LABEL_SEPARATOR);
    if (address == NULL)
    {
        return NULLPTR(EPATH);
    }
    address += 1;

    if (vfs_compare_labels(domain, DOMAIN_LOCAL))
    {
        return net_annouce_local(address);
    }
    else
    {
        return NULLPTR(EPATH);
    }
}

File* net_dial(const char* address)
{
    return NULLPTR(EIMPL);
}