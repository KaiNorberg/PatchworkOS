#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    const char* name;
    const char* synopsis;
    const char* description;
    uint64_t (*callback)(uint64_t argc, const char** argv);
} builtin_t;

bool builtin_exists(const char* name);

uint64_t builtin_execute(uint64_t argc, const char** argv);
