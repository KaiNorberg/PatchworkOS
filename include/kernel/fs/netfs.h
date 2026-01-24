#pragma once

#include <kernel/fs/path.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/rwmutex.h>
#include <kernel/utils/ref.h>

#include <stdint.h>
#include <sys/fs.h>
#include <sys/list.h>

typedef struct netfs_family netfs_family_t;

/**
 * @brief Networking and Sockets.
 * @defgroup kernel_fs_netfs Networking Filesystem
 * @ingroup kernel_fs
 *
 * The networking filesystem provides networking and socket IPC functionality to the operating system. It exposes a
 * common interface for various networking protocols and inter-process communication (IPC) mechanisms.
 *
 * ## Network Families
 *
 * Network families represent different networking protocols or IPC mechanisms. Each family has its own directory in the
 * filesystem, named after the family.
 *
 * Each family directory contains factory files for creating sockets of different types, including `stream`, `dgram`,
 * `seqpacket`, `raw`, and `rdm`.
 *
 * Additionally, there is an `addrs` file that lists the addresses of all listening sockets within that family in the
 * format:
 *
 * ```
 * <address>\n<address>\n...
 * ```
 *
 * ## Sockets
 *
 * Sockets are created by opening a factory file, named after the socket type it will create, located in each socket
 * family's directory. Once a socket is created, it will persist until the namespace that created it is destroyed and
 * there are no more references to it.
 *
 * For example, to create a local seqpacket socket, open the `/local/seqpacket` file. This returns a handle that when
 * read returns the socket's ID, which corresponds to the path `/<family_name>/<socket_id>/`, for example
 * `/local/1234/`, which stores the files used to interact with the socket.
 *
 * The socket directory will only be visible in the namespace that created it.
 *
 * The files used to interact with sockets are listed below.
 *
 * ### accept
 *
 * The `/<family_name>/<socket_id>/accept` file can be opened on a listening socket to accept incoming connections.
 * Working in an similar way to the POSIX `accept()` function, the returned file descriptor represents the new
 * connection.
 *
 * If opened with `:nonblock` and there are no incoming connections, the open will fail with `EAGAIN`, otherwise it will
 * block until a connection is available.
 *
 * ### ctl
 *
 * The `/<family_name>/<socket_id>/ctl` file is used to send "commands" to the socket by writing to it. Here is a
 * list of supported commands:
 * - `bind <address>`: Binds the socket to the specified address. (POSIX `bind()` function)
 * - `listen <backlog>`: Puts the socket into listening mode with the specified backlog length. (POSIX `listen()`
 * function)
 * - `connect <address>`: Connects the socket to the specified address. (POSIX `connect()` function)
 *
 * ### data
 *
 * The `/<family_name>/<socket_id>/data` file is used to send and receive data using the socket. Writing to this
 * file sends data, reading from it receives data. (POSIX `send()` and `recv()` functions)
 *
 * If opened with `:nonblock`, read and write operations will fail with `EAGAIN` if no data is available or there is no
 * buffer space available, respectively. If not opened with `:nonblock` they will block, waiting for data or buffer
 * space.
 *
 * @{
 */

/**
 * @brief The name of the networking filesystem.
 */
#define NETFS_NAME "netfs"

/**
 * @brief The default backlog size for listening sockets.
 */
#define NETFS_BACKLOG_DEFAULT 128

/**
 * @brief Socket types.
 * @enum socket_type_t
 */
typedef enum
{
    SOCKET_STREAM = 1 << 0,    ///< A sequenced, reliable, two-way connection-based byte stream.
    SOCKET_DGRAM = 1 << 1,     ///< A connectionless, unreliable datagram service.
    SOCKET_SEQPACKET = 1 << 2, ///< A sequenced, reliable, two-way connection-based packet stream.
    SOCKET_RAW = 1 << 3,       ///< Provides raw network protocol access.
    SOCKET_RDM = 1 << 4,       ///< A reliable datagram layer that does not guarantee ordering.
    SOCKET_TYPE_AMOUNT = 5,
} socket_type_t;

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
    SOCKET_CLOSED
} socket_state_t;

/**
 * @brief Socket structure.
 * @struct socket_t
 */
typedef struct socket
{
    ref_t ref;
    list_entry_t listEntry;
    char id[MAX_NAME];
    char address[MAX_PATH];
    netfs_family_t* family;
    socket_type_t type;
    socket_state_t state;
    weak_ptr_t ownerNs; ///< A weak pointer to the namespace that created the socket.
    void* data;
    mutex_t mutex;
} socket_t;

/**
 * @brief Socket Family structure.
 * @struct netfs_family_t
 */
typedef struct netfs_family
{
    const char* name;
    /**
     * @brief Initialize a socket.
     *
     * @param sock Pointer to the socket to initialize.
     * @return On success, `0`. On failure, `ERR` and `errno` is set.
     */
    uint64_t (*init)(socket_t* sock);
    /**
     * @brief Deinitialize a socket.
     *
     * @param sock Pointer to the socket to deinitialize.
     */
    void (*deinit)(socket_t* sock);
    /**
     * @brief Bind a socket to its address.
     *
     * The address is stored in `socket_t::address`.
     *
     * @param sock Pointer to the socket to bind.
     * @return On success, `0`. On failure, `ERR` and `errno` is set.
     */
    uint64_t (*bind)(socket_t* sock);
    /**
     * @brief Listen for incoming connections on a socket.
     *
     * @param sock Pointer to the socket to listen on.
     * @param backlog Maximum number of pending connections.
     * @return On success, `0`. On failure, `ERR` and `errno` is set.
     */
    uint64_t (*listen)(socket_t* sock, uint32_t backlog);
    /**
     * @brief Connect a socket to its address.
     *
     * The address is stored in `socket_t::address`.
     *
     * @param sock Pointer to the socket to connect.
     * @return On success, `0`. On failure, `ERR` and `errno` is set.
     */
    uint64_t (*connect)(socket_t* sock);
    /**
     * @brief Accept an incoming connection on a listening socket.
     *
     * @param sock Pointer to the listening socket.
     * @param newSock Pointer to the socket to initialize for the new connection.
     * @param mode Mode flags for the new socket.
     * @return On success, `0`. On failure, `ERR` and `errno` is set.
     */
    uint64_t (*accept)(socket_t* sock, socket_t* newSock, mode_t mode);
    /**
     * @brief Send data on a socket.
     *
     * @param sock Pointer to the socket to send data on.
     * @param buffer Pointer to the data to send.
     * @param count Number of bytes to send.
     * @param offset Pointer to the position in the file, families may ignore this.
     * @param mode Mode flags for sending.
     * @return On success, number of bytes sent. On failure, `ERR` and `errno` is set.
     */
    size_t (*send)(socket_t* sock, const void* buffer, size_t count, size_t* offset, mode_t mode);
    /**
     * @brief Receive data on a socket.
     *
     * @param sock Pointer to the socket to receive data on.
     * @param buffer Pointer to the buffer to store received data.
     * @param count Maximum number of bytes to receive.
     * @param offset Pointer to the position in the file, families may ignore this.
     * @param mode Mode flags for receiving.
     * @return On success, number of bytes received. On failure, `ERR` and `errno` is set.
     */
    size_t (*recv)(socket_t* sock, void* buffer, size_t count, size_t* offset, mode_t mode);
    /**
     * @brief Poll a socket for events.
     *
     * @param sock Pointer to the socket to poll.
     * @param revents Pointer to store the events that occurred.
     * @return On success, a pointer to the wait queue to block on. On failure, `NULL` and `errno` is set.
     */
    wait_queue_t* (*poll)(socket_t* sock, poll_events_t* revents);
    list_entry_t listEntry;
    list_t sockets;
    rwmutex_t mutex;
} netfs_family_t;

/**
 * @brief Initialize the networking filesystem.
 */
void netfs_init(void);

/**
 * @brief Register a network family.
 *
 * @param family Pointer to the network family structure.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t netfs_family_register(netfs_family_t* family);

/**
 * @brief Unregister a network family.
 *
 * @param family Pointer to the network family structure.
 */
void netfs_family_unregister(netfs_family_t* family);

/** @} */