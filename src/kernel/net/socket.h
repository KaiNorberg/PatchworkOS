#pragma once

#include "fs/path.h"
#include "socket_type.h"

#include <sys/io.h>

typedef struct socket_family socket_family_t;

typedef enum
{
    SOCKET_STATE_NEW,
    SOCKET_STATE_BOUND,
    SOCKET_STATE_LISTENING,
    SOCKET_STATE_CONNECTING,
    SOCKET_STATE_CONNECTED,
    SOCKET_STATE_CLOSING,
    SOCKET_STATE_CLOSED,
    SOCKET_STATE_ERROR
} socket_state_t;

typedef struct socket
{
    socket_state_t state;
    char id[MAX_NAME];
} socket_t;

socket_t* socket_new(socket_family_t* family, socket_type_t type, path_flags_t flags)
