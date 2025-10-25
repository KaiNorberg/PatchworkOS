#pragma once

#include "fs/path.h"
#include "fs/sysfs.h"
#include "socket_type.h"
#include "sync/rwmutex.h"
#include "utils/ref.h"

#include <sys/io.h>

typedef struct socket_family socket_family_t;

/**
 * @brief Sockets.
 * @defgroup kernel_net_socket Sockets
 * @ingroup kernel_net
 *
 * Sockets are exposed in the `/net` directory. Sockets provide communication endpoints for networking.
 *
 * ## Creating Sockets
 *
 * Sockets are created by opening a factory located in each socket families directory. For example, to create a local
 * seqpacket socket, open the `/net/local/seqpacket/` file which gives you a handle that when read returns the socket's
 * ID, which corresponds to the path `/net/<family_name>/<socket_id>/`, for example `/net/local/1234/`, which stores the
 * files used to interact with the socket.
 *
 * ## Using Sockets
 *
 * Sockets are interacted with using the following files located in their directory.
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
 * The `/net/<family_name>/<socket_id>/ctl` file is used to send "commands" to the socket. Here is a list of supported
 * commands:
 * - `bind <address>`: Binds the socket to the specified address. (POSIX `bind()` function)
 * - `listen <backlog>`: Puts the socket into listening mode with the specified backlog length. (POSIX `listen()`
 * function)
 * - `connect <address>`: Connects the socket to the specified address. (POSIX `connect()` function)
 *
 * ### data
 *
 * The `/net/<family_name>/<socket_id>/data` file is used to send and retrieve data using the socket. Writing to this
 * file sends data, reading from it receives data. (POSIX `send()` and `recv()` functions)
 *
 * If opened with `:nonblock`, read and write operations will fail with `EAGAIN` if no data is available or there is no
 * buffer space available, respectively, otherwise they will block, waiting for data or buffer space.
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
    ref_t ref;
    char id[MAX_NAME];
    char address[MAX_NAME];
    socket_family_t* family;
    socket_type_t type;
    path_flags_t flags;
    void* private;
    socket_state_t currentState;
    socket_state_t nextState;
    rwmutex_t mutex;
    dentry_t* ctlFile;
    dentry_t* dataFile;
    dentry_t* acceptFile;
} socket_t;

/**
 * @brief Create a new socket.
 *
 * There is no `socket_free()` function, instead use `DEREF()` to free the socket.
 *
 * @param family Pointer to the socket family.
 * @param type Socket type.
 * @param flags Path flags.
 * @return On success, pointer to the new socket. On failure, `NULL` and `errno` is set.
 */
socket_t* socket_new(socket_family_t* family, socket_type_t type, path_flags_t flags);

/**
 * @brief Starts a socket state transition.
 *
 * @param sock Pointer to the socket.
 * @param state Target state.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t socket_start_transition(socket_t* sock, socket_state_t state);

/**
 * @brief Without releasing the socket mutex, start a transition to a new target state.
 *
 * @param sock Pointer to the socket.
 * @param state Target state.
 */
void socket_continue_transition(socket_t* sock, socket_state_t state);

/**
 * @brief Ends a socket state transition.
 *
 * @param sock Pointer to the socket.
 * @param result Result of the transition, if `ERR` the transition failed.
 */
void socket_end_transition(socket_t* sock, uint64_t result);

/** @} */
