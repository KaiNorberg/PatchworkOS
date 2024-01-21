#include "vector.h"

#include "heap/heap.h"
#include "string/string.h"

Vector* vector_new(uint64_t entrySize)
{
    Vector* vec = kmalloc(sizeof(Vector));

    vec->data = kmalloc(entrySize * VECTOR_INITIAL_LENGTH);
    vec->entrySize = entrySize;

    vec->length = 0;
    vec->reservedLength = VECTOR_INITIAL_LENGTH;

    return vec;
}

uint64_t vector_length(Vector* vec)
{
    return vec->length;
}

void vector_push(Vector* vec, void* entry)
{
    if (vec->length == vec->reservedLength)
    {
        uint64_t newLength = vec->length * 2;
        void* newData = kmalloc(newLength * vec->entrySize);
        memcpy(newData, vec->data, vec->length * vec->entrySize);
        kfree(vec->data);

        vec->data = newData;
        vec->reservedLength = newLength;
    }

    memcpy(vector_get(vec, vec->length), entry, vec->entrySize);
    vec->length++;
}

void* vector_insert(Vector* vec, uint64_t index)
{
    return (void*)((uint64_t)vec->data + vec->entrySize * index);
}

void* vector_array(Vector* vec)
{
    return vec->data;
}

void* vector_get(Vector* vec, uint64_t index)
{
    return (void*)((uint64_t)vec->data + vec->entrySize * index);
}