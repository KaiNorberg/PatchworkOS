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

typedef struct local_listener local_listener_t;

typedef struct local_connection
{
    ring_t serverRing;
    ring_t clientRing;
    local_listener_t* listener;
    lock_t lock;
    wait_queue_t waitQueue;
    atomic_uint64 ref;
    atomic_bool accepted;
} local_connection_t;

typedef struct local_listener
{
    list_entry_t entry;
    char address[MAX_NAME];
    local_connection_t* backlog[LOCAL_BACKLOG_MAX];
    uint64_t readIndex;
    uint64_t writeIndex;
    uint64_t length;
    lock_t lock;
    wait_queue_t waitQueue;
    atomic_uint64 ref;
    sysobj_t* obj;
} local_listener_t;

typedef struct local_packet_header
{
    uint64_t size; // Size not including header
} local_packet_header_t;

typedef struct local_socket
{
    local_socket_state_t state;
    union {
        struct
        {
            char address[MAX_NAME];
        } bind;
        struct
        {
            local_listener_t* listener;
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
