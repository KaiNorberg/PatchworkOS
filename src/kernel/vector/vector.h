#pragma once

#include <stdint.h>

#define VECTOR_INITIAL_LENGTH 4

typedef struct
{
    void* data;
    uint64_t entrySize;
    
    uint64_t length;
    uint64_t reservedLength;
} Vector;

Vector* vector_new(uint64_t entrySize);

uint64_t vector_length(Vector* vec);

void vector_push(Vector* vec, void* entry);

void* vector_get(Vector* vec, uint64_t index);

