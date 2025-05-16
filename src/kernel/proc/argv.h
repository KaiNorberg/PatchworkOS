#pragma once

#include <stdint.h>

typedef struct
{
    char** buffer; // Stores both pointers and strings like "[ptr1][ptr2][ptr3][NULL][string1][string2][string3]"
    uint64_t size;
    uint64_t amount;
} argv_t;

uint64_t argv_init(argv_t* argv, const char** src);

void argv_deinit(argv_t* argv);
