#pragma once

#include "fs/sysfs.h"
#include "socket_type.h"

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct socket socket_t;
typedef struct socket_family socket_family_t;

/**
 * @brief Socket Families.
 * @defgroup kernel_net_socket_family Socket Families
 * @ingroup kernel_net
 *
 * A socket family defines a set of operations for a socket, defining what it means to read, write, etc., for that
 * specific family.
 *
 * Each socket family is exposed as `/net/<family_name>/`.
 *
 * @{
 */

/**
 * @brief Socket Factory structure.
 * @struct socket_factory_t
 *
 * A socket factory is used to create sockets of a specific type within a socket family.
 *
 * Each factory is exposed as `/net/<family_name>/<socket_type>/`.
 *
 */
typedef struct socket_factory
{
    list_entry_t entry;
    socket_type_t type;
    socket_family_t* family;
    dentry_t* file;
} socket_factory_t;

/**
 * @brief Socket Family operations structure.
 * @struct socket_family_ops_t
 */
typedef struct socket_family_ops
{
    uint64_t (*init)(socket_t* sock);
    void (*deinit)(socket_t* sock);
    uint64_t (*bind)(socket_t* sock, const char* address);
    uint64_t (*listen)(socket_t* sock, uint32_t backlog);
    uint64_t (*connect)(socket_t* sock, const char* address);
    uint64_t (*accept)(socket_t* sock, socket_t* newSock);
    uint64_t (*send)(socket_t* sock, const void* buffer, uint64_t count, uint64_t* offset);
    uint64_t (*recv)(socket_t* sock, void* buffer, uint64_t count, uint64_t* offset);
    wait_queue_t* (*poll)(socket_t* sock, poll_events_t* revents);
    uint64_t (*shutdown)(socket_t* socket, uint32_t how); // TODO: This is not used nor implemented, implement it.
} socket_family_ops_t;

/**
 * @brief Socket Family structure.
 * @struct socket_family_t
 */
typedef struct socket_family
{
    list_entry_t entry;
    char name[MAX_NAME];
    const socket_family_ops_t* ops;
    socket_type_t supportedTypes;
    atomic_uint64_t newId;
    list_t factories;
    dentry_t* dir;
} socket_family_t;

/**
 * @brief Register a socket family.
 *
 * @param name Name of the socket family, must be unique.
 * @param ops Pointer to the socket family operations.
 * @param supportedTypes Supported socket types (bitmask of `socket_type_t`).
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t socket_family_register(const socket_family_ops_t* ops, const char* name, socket_type_t supportedTypes);

/**
 * @brief Unregister a socket family.
 *
 * @param name Name of the socket family to unregister.
 */
void socket_family_unregister(const char* name);

/**
 * @brief Get a socket family by name.
 *
 * @param name Name of the socket family to get.
 * @return Pointer to the socket family or `NULL`.
 */
socket_family_t* socket_family_get(const char* name);

/**
 * @brief Get the directory of a socket family.
 *
 * @param family Pointer to the socket family.
 * @param outPath Output path for the directory (`/net/<family_name>/`).
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t socket_family_get_dir(socket_family_t* family, path_t* outPath);

/** @} */
