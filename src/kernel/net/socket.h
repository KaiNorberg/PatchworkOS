#pragma once

#include "fs/path.h"
#include "fs/sysfs.h"
#include "socket_type.h"
#include "utils/ref.h"
#include "sync/rwmutex.h"

#include <sys/io.h>

typedef struct socket_family socket_family_t;

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

typedef struct socket
{
    ref_t ref;
    char id[MAX_NAME];
    char address[MAX_NAME];
    socket_family_t* family;
    socket_type_t type;
    path_flags_t flags;
    pid_t creator;
    void* private;
    socket_state_t currentState;
    socket_state_t nextState;
    rwmutex_t mutex;
    bool isExposed;
    sysfs_dir_t dir;
    sysfs_file_t ctlFile;
    sysfs_file_t dataFile;
    sysfs_file_t acceptFile;
} socket_t;

socket_t* socket_new(socket_family_t* family, socket_type_t type, path_flags_t flags);
void socket_free(socket_t* sock);

uint64_t socket_expose(socket_t* sock);
void socket_hide(socket_t* sock);

uint64_t socket_start_transition(socket_t* sock, socket_state_t state);
void socket_continue_transition(socket_t* sock, socket_state_t state);
void socket_end_transition(socket_t* sock, uint64_t result);
