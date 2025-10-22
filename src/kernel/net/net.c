#include "net.h"

#include "fs/dentry.h"
#include "fs/mount.h"
#include "fs/sysfs.h"
#include "log/log.h"
#include "log/panic.h"
#include "net/local/local.h"

static mount_t* mount;

void net_init(void)
{
    mount = sysfs_mount_new(NULL, "net", NULL, NULL);
    if (mount == NULL)
    {
        panic(NULL, "Failed to create /net filesystem");
    }

    net_local_init();

    LOG_INFO("networking initialized\n");
}

mount_t* net_get_mount(void)
{
    return REF(mount);
}
