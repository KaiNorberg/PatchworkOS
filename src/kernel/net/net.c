#include "net.h"

#include "fs/sysfs.h"
#include "local.h"
#include "log/log.h"

static sysfs_group_t group;

void net_init(void)
{
    LOG_INFO("net: init\n");

    assert(sysfs_group_init(&group, PATHNAME("/net")) != ERR);

    net_local_init();
}

sysfs_dir_t* net_get_dir(void)
{
    return &group.root;
}
