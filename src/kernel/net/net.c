#include <kernel/net/net.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/sysfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/net/local/local.h>

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
