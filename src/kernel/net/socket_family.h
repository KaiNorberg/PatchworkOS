#pragma once

#include "fs/sysfs.h"
#include "socket_type.h"

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct socket socket_t;
typedef struct socket_family socket_family_t;

typedef struct socket_factory
{
    list_entry_t entry;
    socket_type_t type;
    socket_family_t* family;
    sysfs_file_t file;
} socket_factory_t;

typedef struct socket_family
{
    const char* name;
    socket_type_t supportedTypes;
    uint64_t (*init)(socket_t* sock);
    void (*deinit)(socket_t* sock);
    uint64_t (*bind)(socket_t* sock, const char* address);
    uint64_t (*listen)(socket_t* sock, uint32_t backlog);
    uint64_t (*connect)(socket_t* sock, const char* address);
    uint64_t (*accept)(socket_t* sock, socket_t* newSock);
    uint64_t (*close)(socket_t* sock);
    uint64_t (*recv)(socket_t* sock, void* buffer, uint64_t count, uint64_t* offset);
    uint64_t (*send)(socket_t* sock, const void* buffer, uint64_t count, uint64_t* offset);
    wait_queue_t* (*poll)(socket_t* sock, poll_events_t events, poll_events_t* occoured);
    atomic_uint64_t newId; //!< Internal.
    sysfs_dir_t dir; //!< Internal.
    list_t factories; //!< Internal.
} socket_family_t;

uint64_t socket_family_register(socket_family_t* family);

void socket_family_unregister(socket_family_t* family);
