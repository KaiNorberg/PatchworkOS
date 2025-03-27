#pragma once

#include <stdint.h>

typedef struct
{
    const char* name;
    const char* synopsis;
    const char* description;
    void (*callback)(uint64_t argc, const char** argv);
} command_t;

void command_execute(const char* command);
