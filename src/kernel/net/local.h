#pragma once

// Note: Local sockets always use SOCK_SEQPACKET

#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "sched/wait.h"
#include "sync/lock.h"
#include "utils/ring.h"

#include <stdint.h>
#include <sys/io.h>

#define LOCAL_BACKLOG_MAX 32
#define LOCAL_BUFFER_SIZE (0x4000)
#define LOCAL_MAX_PACKET_SIZE (LOCAL_BUFFER_SIZE - sizeof(local_packet_header_t))

typedef enum
{
    LOCAL_SOCKET_BLANK,
    LOCAL_SOCKET_BOUND,
    LOCAL_SOCKET_LISTEN,
    LOCAL_SOCKET_CONNECT,
    LOCAL_SOCKET_ACCEPT,
} local_socket_state_t;

typedef struct
{
    uint64_t size;
} local_packet_header_t;

typedef struct local_listener local_listener_t;

typedef struct
{
    ring_t serverToClient;
    ring_t clientToServer;
    local_listener_t* listener;
    lock_t lock;
    wait_queue_t waitQueue;
    atomic_uint64_t ref;
    atomic_bool isAccepted;
} local_connection_t;

typedef struct
{
    local_connection_t* buffer[LOCAL_BACKLOG_MAX];
    uint64_t readIndex;
    uint64_t writeIndex;
    uint64_t count;
} local_backlog_t;

typedef struct local_listener
{
    list_entry_t entry;
    char address[MAX_NAME];
    local_backlog_t backlog;
    lock_t lock;
    wait_queue_t waitQueue;
    atomic_uint64_t ref;
    sysfile_t sysfile;
} local_listener_t;

typedef struct
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
