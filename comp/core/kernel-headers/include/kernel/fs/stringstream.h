#pragma once

#include <kernel/fs/vfs.h>
#include <kernel/io/irp.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/fifo.h>
#include <libstd/list.h>

/**
 * @brief Helper for creating files providing a stream of strings.
 * @defgroup kernel_utils_stringstream
 * @ingroup kernel_utils
 *
 * A stringstream is a utility for managing a set of clients that receive a stream of messages.
 *
 * Each client maintains its own FIFO buffer of messages, and messages can be broadcast to all
 * currently connected clients.
 *
 * To implement a stringstream simply create a vnode class with the `STRINGSTREAM_HANDLERS()` set and with the vnodes
 * private data set to a pointer to a `stringstream_t`.
 *
 * @{
 */

/**
 * @brief Size of the stringstream client data buffer.
 */
#define STRINGSTREAM_BUFFER_SIZE 7622

/**
 * @brief Maximum number of messages a stringstream client can hold.
 */
#define STRINGSTREAM_MAX_MESSAGES 256

/**
 * @brief Stringstream client structure.
 * @struct stringstream_client_t
 */
typedef struct stringstream_client
{
    list_entry_t entry;
    FIFO_DEFINE(fifo, STRINGSTREAM_BUFFER_SIZE);
    uint16_t lengths[STRINGSTREAM_MAX_MESSAGES];
    uint16_t head;
    uint16_t tail;
} stringstream_client_t;

/**
 * @brief Stringstream structure.
 * @struct stringstream_t
 */
typedef struct stringstream
{
    list_t pending;
    list_t clients;
    lock_t lock;
} stringstream_t;

/**
 * @brief Create a stringstream initializer.
 *
 * @param _stream The name of the stringstream variable.
 */
#define STRINGSTREAM_CREATE(_stream) \
    { \
        .pending = LIST_CREATE((_stream).pending), \
        .clients = LIST_CREATE((_stream).clients), \
        .lock = LOCK_CREATE(), \
    }

/**
 * @brief Initialize a stringstream.
 *
 * @param stream The stream to initialize.
 */
void stringstream_init(stringstream_t* stream);

/**
 * @brief Deinitialize a stringstream.
 *
 * @param stream The stream to deinitialize.
 */
void stringstream_deinit(stringstream_t* stream);

/**
 * @brief Open handler for stringstream.
 */
status_t stringstream_open(irp_t* irp);

/**
 * @brief Close handler for stringstream.
 */
status_t stringstream_close(irp_t* irp);

/**
 * @brief Read handler for stringstream.
 */
status_t stringstream_read(irp_t* irp);

/**
 * @brief Poll handler for stringstream.
 */
status_t stringstream_poll(irp_t* irp);

/**
 * @brief Broadcast a string to all clients of the stream.
 *
 * @param stream The stream.
 * @param string The string to broadcast.
 * @param length The length of the string.
 */
void stringstream_broadcast(stringstream_t* stream, const char* string, size_t length);

/**
 * @brief Helper macro to define stringstream vnode handlers.
 */
#define STRINGSTREAM_HANDLERS() \
    [IRP_MJ_OPEN] = stringstream_open, [IRP_MJ_READ] = stringstream_read, [IRP_MJ_POLL] = stringstream_poll, \
    [IRP_MJ_CLOSE] = stringstream_close

/** @} */