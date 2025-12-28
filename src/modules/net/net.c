#include "net.h"

#include "local/local.h"
#include "socket_family.h"

#include <kernel/fs/dentry.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/sysfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <sys/io.h>

static mount_t* net;

mount_t* net_get_mount(void)
{
    return REF(net);
}

static uint64_t net_init(void)
{
    net = sysfs_mount_new("net", NULL, MODE_PROPAGATE_CHILDREN | MODE_PROPAGATE_PARENTS | MODE_ALL_PERMS, NULL, NULL,
        NULL);
    if (net == NULL)
    {
        return ERR;
    }

    if (net_local_init() == ERR)
    {
        UNREF(net);
        return ERR;
    }

    return 0;
}

static void net_deinit(void)
{
    net_local_deinit();

    socket_family_unregister_all();

    UNREF(net);
}

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        net_init();
        break;
    case MODULE_EVENT_UNLOAD:
        net_deinit();
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Networking", "Kai Norberg", "Provides networking and socket IPC functionality", OS_VERSION, "MIT",
    "BOOT_ALWAYS");