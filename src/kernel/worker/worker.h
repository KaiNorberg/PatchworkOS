#pragma once

#include <stdint.h>

#define MAX_WORKER_AMOUNT 255

typedef struct
{
    uint8_t present;
    uint8_t running;
    uint8_t id; 
    uint8_t apicId;
} Worker;

void worker_entry();