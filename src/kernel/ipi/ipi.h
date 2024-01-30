#pragma once

#include <stdint.h>

#define IPI_VECTOR 0x90

typedef struct
{
    uint8_t type;
} Ipi;