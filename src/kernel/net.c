#include "net.h"

#include <errno.h>

#include "sched.h"

/*static uint64_t server_local_read(File* file, void* buffer, uint64_t count)
{

}*/

/*static void server_local_cleanup(File* file)
{
    
}*/

static File* net_annouce_local(const char* address)
{
    if (!name_is_last(address) || !name_valid(address))
    {
        return NULLPTR(EPATH);
    }

    //File* file = file_new();
    //file->cleanup = server_local_cleanup;

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

    if (label_compare(domain, DOMAIN_LOCAL))
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