#include "net.h"

#include "fs/dentry.h"
#include "fs/mount.h"
#include "fs/superblock.h"
#include "fs/sysfs.h"
#include "log/log.h"
#include "log/panic.h"
#include "net/local/local.h"

static superblock_t* superblock;

void net_init(void)
{
    superblock = sysfs_superblock_new(NULL, "net", NULL, NULL);
    if (superblock == NULL)
    {
        panic(NULL, "Failed to create /net filesystem");
    }

    net_local_init();

    LOG_INFO("networking initialized\n");
}

void net_get_dir(void)
{
    return superblock->root;
}
