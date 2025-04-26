#pragma once

#include "sysfs.h"

#include <stdint.h>

typedef struct socket socket_t;

typedef uint64_t (*socket_init_t)(socket_t*);
typedef void (*socket_deinit_t)(socket_t*);
typedef uint64_t (*socket_bind_t)(socket_t*, const char*);
typedef uint64_t (*socket_listen_t)(socket_t*);
typedef uint64_t (*socket_accept_t)(socket_t*, socket_t*);
typedef uint64_t (*socket_connect_t)(socket_t*, const char*);
typedef uint64_t (*socket_send_t)(socket_t*, const void*, uint64_t);
typedef uint64_t (*socket_receive_t)(socket_t*, void*, uint64_t, uint64_t);

// Note: All functions must be implemented.
typedef struct
{
    const char* name;
    socket_init_t init;
    socket_deinit_t deinit;
    socket_bind_t bind;
    socket_listen_t listen;
    socket_connect_t connect;
    socket_accept_t accept;
    socket_send_t send;
    socket_receive_t receive;
} socket_family_t;

typedef struct
{
    char id[MAX_NAME];
    sysdir_t* dir;
} socket_handle_t;

sysdir_t* socket_family_expose(socket_family_t* family);
