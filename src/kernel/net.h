#pragma once

#include <stdint.h>

#include "vfs.h"
#include "lock.h"

#define DOMAIN_LOCAL "local"

typedef struct
{

} Server;

typedef struct
{

} Client;

File* net_announce(const char* address);

File* net_dial(const char* address);