#pragma once

#include <stdint.h>

#include "socket_family.h"
#include "sync/lock.h"

typedef struct socket
{
    char id[MAX_NAME];
    void* private;
    socket_family_t* family;
    pid_t creator;
    path_flags_t flags;
    sysfs_dir_t dir;
    sysfs_file_t ctlFile;
    sysfs_file_t dataFile;
    sysfs_file_t acceptFile;
} socket_t;

socket_t* socket_new(socket_family_t* family, path_flags_t flags);

void socket_free(socket_t* socket);