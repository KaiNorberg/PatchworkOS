#include "local.h"

#include "log.h"
#include "socket.h"

#include <stdlib.h>

static sysdir_t* sysdir;

static uint64_t local_socket_init(socket_t* socket)
{
    socket_local_t* private = malloc(sizeof(socket_local_t));
    if (private == NULL)
    {
        return ERR;
    }
    socket->state = SOCKET_BLANK;
    socket->private = private;
    lock_init(&socket->lock);

    return 0;
}

static void local_socket_deinit(socket_t* socket)
{
    socket_local_t* private = socket->private;
    free(private);
}

static socket_family_t family =
{
    .name = "local",
    .init = local_socket_init,
    .deinit = local_socket_deinit,
};

void net_local_init(void)
{
    sysdir = socket_family_expose(&family);
    ASSERT_PANIC(sysdir != NULL);
}
