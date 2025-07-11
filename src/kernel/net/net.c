#include "net.h"

#include "fs/sysfs.h"
#include "net/local/local.h"
#include "log/log.h"
#include "log/panic.h"

static sysfs_group_t group;

void net_init(void)
{
    LOG_INFO("net: init\n");

    if (sysfs_group_init(&group, PATHNAME("/net")) == ERR)
    {
        panic(NULL, "Failed to initialize network sysfs group");
    }

    net_local_init();
}

sysfs_dir_t* net_get_dir(void)
{
    return &group.root;
}
