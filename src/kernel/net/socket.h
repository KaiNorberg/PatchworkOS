#pragma once

#include <stdint.h>

#include "lock.h"
#include "socket_family.h"

typedef enum
{
    SOCKET_BLANK,
    SOCKET_BOUND,
    SOCKET_LISTENING,
    SOCKET_CONNECTED,
    SOCKET_ACCEPTED
} socket_state_t;

typedef struct socket
{
    void* private;
    socket_family_t* family;
    socket_state_t state;
    pid_t creator;
    lock_t lock;
} socket_t;

sysdir_t* socket_create(socket_family_t* family, const char* id);
