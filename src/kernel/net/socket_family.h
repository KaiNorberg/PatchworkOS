#pragma once

#include "fs/file.h"
#include "fs/sysfs.h"

#include <stdint.h>

typedef struct socket socket_t;

typedef uint64_t (*socket_init_t)(socket_t*);
typedef void (*socket_deinit_t)(socket_t*);
typedef uint64_t (*socket_bind_t)(socket_t*, const char*);
typedef uint64_t (*socket_listen_t)(socket_t*);
typedef uint64_t (*socket_accept_t)(socket_t*, socket_t*);
typedef uint64_t (*socket_connect_t)(socket_t*, const char*);
typedef uint64_t (*socket_send_t)(socket_t*, const void*, uint64_t, uint64_t*);
typedef uint64_t (*socket_receive_t)(socket_t*, void*, uint64_t, uint64_t*);
typedef wait_queue_t* (*socket_poll_t)(socket_t*, poll_file_t*);

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
    socket_poll_t poll;
    sysfs_dir_t dir;
    sysfs_file_t newFile;
} socket_family_t;

uint64_t socket_family_register(socket_family_t* family);
