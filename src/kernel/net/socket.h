#pragma once

#include <stdint.h>

#include "sync/lock.h"
#include "socket_family.h"

typedef struct socket
{    
    char id[MAX_NAME];
    void* private;
    socket_family_t* family;
    pid_t creator;
    path_flags_t flags;
    sysdir_t dir;
    sysobj_t ctlObj;
    sysobj_t dataObj;
    sysobj_t acceptObj;
} socket_t;

socket_t* socket_new(socket_family_t* family, path_flags_t flags);

void socket_free(socket_t* socket);