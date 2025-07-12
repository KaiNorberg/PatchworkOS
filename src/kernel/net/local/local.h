#pragma once

#include "sync/lock.h"

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

#define LOCAL_BUFFER_SIZE (4 * PAGE_SIZE)
#define LOCAL_MAX_PACKET_SIZE (LOCAL_BUFFER_SIZE - sizeof(local_packet_header_t))
#define LOCAL_MAX_BACKLOG 128

#define LOCAL_PACKET_MAGIC 0xC0D74B56

typedef struct
{
    uint32_t magic;
    uint32_t size;
} local_packet_header_t;

typedef struct local_listen local_listen_t;
typedef struct local_conn local_conn_t;

typedef struct
{
    union {
        struct
        {
            local_listen_t* listen;
        } listen;
        struct
        {
            local_conn_t* conn;
            bool isServer;
        } conn;
    };
    lock_t lock;
} local_socket_data_t;

void net_local_init(void);
