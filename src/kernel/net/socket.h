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
