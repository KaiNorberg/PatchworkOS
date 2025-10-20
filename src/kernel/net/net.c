#include "net.h"

#include "fs/sysfs.h"
#include "log/log.h"
#include "log/panic.h"
#include "net/local/local.h"

static sysfs_group_t group;

void net_init(void)
{
    if (sysfs_group_init(&group, NULL, "net", NULL) == ERR)
    {
        panic(NULL, "Failed to initialize network sysfs group");
    }

    net_local_init();

    LOG_INFO("networking initialized\n");
}

sysfs_dir_t* net_get_dir(void)
{
    return &group.root;
}
