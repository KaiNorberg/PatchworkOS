#pragma once

#include <stdint.h>

#include "lock.h"
#include "socket_family.h"

typedef struct socket
{
    void* private;
    socket_family_t* family;
    pid_t creator;
} socket_t;

sysdir_t* socket_create(socket_family_t* family, const char* id);
