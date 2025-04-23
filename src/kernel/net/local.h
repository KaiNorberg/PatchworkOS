#pragma once

// Note: Local sockets always use SOCK_SEQPACKET

#include <stdint.h>

typedef struct
{
    uint64_t empty;
} socket_local_t;

void net_local_init(void);
