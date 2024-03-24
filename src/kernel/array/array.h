#pragma once

#include "types/types.h"
#include "lock/lock.h"

//Note: Named array to avoid confusion with interrupt vectors

#define ARRAY_INIT_CAPACITY 4

#define ARRAY_FIND_NOT_FOUND 0
#define ARRAY_FIND_FOUND 1

#define ARRAY_ITERATE_CONTINUE 0
#define ARRAY_ITERATE_BREAK 1
#define ARRAY_ITERATE_ERASE 2

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

void* array_find(Array* array, uint64_t(*callback)(void*, void*), void* context);

bool array_iterate(Array* array, uint64_t(*callback)(void*));