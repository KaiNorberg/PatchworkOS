#pragma once

#include <stdint.h>

#define IPI_VECTOR 0x90

#define IPI_TYPE_NONE 0
#define IPI_TYPE_HALT 1
#define IPI_TYPE_SCHEDULE 2

typedef struct
{
    uint8_t type;
} Ipi;