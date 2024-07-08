#pragma once

#include <stdatomic.h>
#include <sys/keyboard.h>
#include <sys/mouse.h>

#include "defs.h"
#include "sysfs.h"

typedef struct
{
    uint64_t writeIndex;
    uint64_t eventSize;
    uint64_t length;
    void* buffer;
    resource_t* resource;
    lock_t lock;
} input_t;

uint64_t input_init(input_t* input, const char* path, const char* name, uint64_t eventSize, uint64_t length);

uint64_t input_cleanup(input_t* input);

uint64_t input_push(input_t* input, const void* event, uint64_t eventSize);
