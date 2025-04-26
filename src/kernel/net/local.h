#pragma once

// Note: Local sockets always use SOCK_SEQPACKET

#include "lock.h"
#include "ring.h"
#include "sysfs.h"
#include "vfs.h"
#include "waitsys.h"

#include <stdint.h>
#include <sys/io.h>

#define LOCAL_BACKLOG_MAX 32

typedef enum
{
    LOCAL_SOCKET_BLANK,
    LOCAL_SOCKET_BOUND,
    LOCAL_SOCKET_LISTEN,
    LOCAL_SOCKET_CONNECT,
    LOCAL_SOCKET_ACCEPT,
} local_socket_state_t;

typedef struct local_connection
{
    ring_t serverRing;
    ring_t clientRing;
    file_t* listener;
    wait_queue_t waitQueue;
    lock_t lock;
    atomic_uint64 ref;
} local_connection_t;

typedef struct
{
    local_socket_state_t state;
    char address[MAX_NAME];
    union
    {
        struct
        {
            sysobj_t* obj;
            local_connection_t* backlog[LOCAL_BACKLOG_MAX];
            uint64_t readIndex;
            uint64_t writeIndex;
            uint64_t length;
            wait_queue_t waitQueue;
        } listen;
        struct
        {
            local_connection_t* conn;
        } connect;
        struct
        {
            local_connection_t* conn;
        } accept;
    };
    lock_t lock;
} local_socket_t;

void net_local_init(void);
