#include "net.h"

#include "local.h"
#include "log/log.h"

void net_init(void)
{
    LOG_INFO("net: init\n");

    net_local_init();
}
