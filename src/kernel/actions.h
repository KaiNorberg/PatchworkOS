#pragma once

#include <stdint.h>

typedef uint64_t (*action_func_t)(uint64_t, const char**, void*);

typedef struct
{
    const char* name;
    action_func_t func;
    uint64_t argcMin;
    uint64_t argcMax;
} action_t;

typedef action_t actions_t[];

uint64_t actions_dispatch(actions_t* actions, const void* buffer, uint64_t count, void* private);
