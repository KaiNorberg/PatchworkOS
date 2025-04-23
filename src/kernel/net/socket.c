#include "socket.h"
#include "defs.h"
#include "sys/atomint.h"
#include "sysfs.h"
#include "vfs.h"
#include "sched.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/math.h>

static file_ops_t dataOps =
{

};

SYSFS_STANDARD_RESOURCE_OPS(dataResOps, &dataOps);

static file_ops_t ctlOps =
{

};

SYSFS_STANDARD_RESOURCE_OPS(ctlResOps, &ctlOps);

static void socket_on_free(sysdir_t* sysdir)
{
    socket_t* socket = sysdir->private;
    socket->family->deinit(socket);
    free(socket);
}

uint64_t socket_create(socket_family_t* family, const char* id)
{
    socket_t* socket = malloc(sizeof(socket_t));
    if (socket == NULL)
    {
        return ERR;
    }
    if (family->init(socket) == ERR)
    {
        free(socket);
        return ERR;
    }

    char path[MAX_PATH];
    sprintf(path, "%s/%s", "/net", family->name);
    sysdir_t* socketDir = sysdir_new(path, id, socket_on_free, socket);
    if (socketDir == NULL)
    {
        family->deinit(socket);
        free(socket);
        return ERR;
    }

    if (sysdir_add(socketDir, "ctl", &ctlResOps, NULL) == ERR || sysdir_add(socketDir, "data", &dataResOps, NULL) == ERR)
    {
        sysdir_free(socketDir);
        return ERR;
    }

    return 0;
}
