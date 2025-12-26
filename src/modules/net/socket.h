#pragma once

#include "socket_type.h"
#include <kernel/fs/path.h>
#include <kernel/fs/sysfs.h>
#include <kernel/sync/rwmutex.h>
#include <kernel/utils/ref.h>

#include <sys/io.h>

typedef struct socket_family socket_family_t;

/**
 * @brief Sockets.
 * @defgroup module_net_socket Sockets
 * @ingroup module_net
 *
 * Sockets provide communication endpoints for networking and local client-server communication. They are exposed in the
 * `/net` directory.
 *
 * ## Creating Sockets
 *
 * Sockets are created by opening a factory file, named after the socket type it will create, located in each socket
 * family's directory. For example, to create a local seqpacket socket, open the `/net/local/seqpacket` file. This
 * returns a handle that when read returns the socket's ID, which corresponds to the path
 * `/net/<family_name>/<socket_id>/`, for example `/net/local/1234/`, which stores the files used to interact with the
 * socket.
 *
 * The sockets file will only be visible within the namespace of the creating process.
 *
 * The files used to interact with sockets are listed below.
 *
 * ### accept
 *
 * The `/net/<family_name>/<socket_id>/accept` file can be opened on a listening socket to accept incoming connections.
 * Working in an similiar way to the POSIX `accept()` function, the returned file descriptor represents the new
 * connection.
 *
 * If opened with `:nonblock` and there are no incoming connections, the open will fail with `EAGAIN`, otherwise it will
 * block until a connection is available.
 *
 * ### ctl
 *
 * The `/net/<family_name>/<socket_id>/ctl` file is used to send "commands" to the socket by writing to it. Here is a
 * list of supported commands:
 * - `bind <address>`: Binds the socket to the specified address. (POSIX `bind()` function)
 * - `listen <backlog>`: Puts the socket into listening mode with the specified backlog length. (POSIX `listen()`
 * function)
 * - `connect <address>`: Connects the socket to the specified address. (POSIX `connect()` function)
 *
 * ### data
 *
 * The `/net/<family_name>/<socket_id>/data` file is used to send and receive data using the socket. Writing to this
 * file sends data, reading from it receives data. (POSIX `send()` and `recv()` functions)
 *
 * If opened with `:nonblock`, read and write operations will fail with `EAGAIN` if no data is available or there is no
 * buffer space available, respectively. If not opened with `:nonblock` they will block, waiting for data or buffer
 * space.
 *
 * @{
 */

/**
 * @brief Socket states.
 * @enum socket_state_t
 */
typedef enum
{
    SOCKET_NEW,
    SOCKET_BOUND,
    SOCKET_LISTENING,
    SOCKET_CONNECTING,
    SOCKET_CONNECTED,
    SOCKET_CLOSING,
    SOCKET_CLOSED,
    SOCKET_STATE_AMOUNT
} socket_state_t;

/**
 * @brief Socket structure.
 * @struct socket_t
 */
typedef struct socket
{
    char id[MAX_NAME];
    char address[MAX_NAME];
    socket_family_t* family;
    socket_type_t type;
    void* private;
    socket_state_t state;
    mutex_t mutex;
    list_t files;
} socket_t;

/**
 * @brief Create a new socket.
 *
 * There is no `socket_free()` function, instead use `UNREF()` to free the socket.
 *
 * @param family Pointer to the socket family.
 * @param type Socket type.
 * @param out Output pointer to store the socket ID.
 * @param outSize Size of the output buffer.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t socket_create(socket_family_t* family, socket_type_t type, char* out, uint64_t outSize);

/** @} */
