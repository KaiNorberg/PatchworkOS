#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    const char* name;
    const char* synopsis;
    const char* description;
    void (*callback)(uint64_t argc, const char** argv);
} builtin_t;

bool builtin_exists(const char* name);

void builtin_execute(uint64_t argc, const char** argv);
