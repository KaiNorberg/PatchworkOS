#pragma once

#include "defs/defs.h"
#include "lock/lock.h"

//Note: Named array to avoid confusion with interrupt vectors.

#define ARRAY_INIT_CAPACITY 4

typedef enum
{
    FIND_NOT_FOUND,
    FIND_FOUND
} FindResult;

typedef enum
{
    ITERATE_CONTINUE,
    ITERATE_BREAK,
    ITERATE_ERASE
} IterResult;

typedef struct
{
    void** data;
    uint64_t capacity;
    uint64_t length;
    Lock lock;
} Array;

Array* array_new();

void array_free(Array* array);

void array_push(Array* array, void* element);

void* array_find(Array* array, FindResult(*callback)(void*, void*), void* context);

bool array_iterate(Array* array, IterResult(*callback)(void*));

uint64_t array_length(Array* array);