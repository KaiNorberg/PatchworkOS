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

void vector_free(Vector* vector);

void vector_resize(Vector* vector, uint64_t length);

void vector_push(Vector* vector, void const* entry);

void* vector_array(Vector* vector);

void vector_set(Vector* vector, uint64_t index, void const* entry);

void* vector_get(Vector* vector, uint64_t index);

void vector_insert(Vector* vector, uint64_t index, void* entry);

void vector_erase(Vector* vector, uint64_t index);

uint64_t vector_length(Vector* vector);