#include "vector.h"

#include "heap/heap.h"
#include "string/string.h"

Vector* vector_new(uint64_t entrySize)
{
    Vector* vector = kmalloc(sizeof(Vector));

    vector->data = kmalloc(VECTOR_INITIAL_LENGTH * entrySize);
    vector->entrySize = entrySize;

    vector->length = 0;
    vector->reservedLength = VECTOR_INITIAL_LENGTH;

    return vector;
}

void vector_free(Vector* vector)
{
    kfree(vector->data);
    kfree(vector);
}

void vector_resize(Vector* vector, uint64_t length)
{
    void* newData = kmalloc(length * vector->entrySize);
    memcpy(newData, vector->data, vector->length * vector->entrySize);
    kfree(vector->data);

    vector->data = newData;
    vector->reservedLength = length;
}

void* vector_back(Vector* vector)
{
    return vector_get(vector, vector->length - 1);
}

void vector_push_back(Vector* vector, void const* entry)
{
    if (vector->length == vector->reservedLength)
    {
        vector_resize(vector, vector->reservedLength * 2);
    }

    vector_set(vector, vector->length, entry);
    vector->length++;
}

void vector_pop_back(Vector* vector, void* dest)
{
    memcpy(dest, vector_get(vector, vector->length - 1), vector->entrySize);
    vector->length--;
}

void* vector_array(Vector* vector)
{
    return vector->data;
}

void vector_set(Vector* vector, uint64_t index, void const* entry)
{
    memcpy(vector_get(vector, index), entry, vector->entrySize);
}

void* vector_get(Vector* vector, uint64_t index)
{
    return (void*)((uint64_t)vector->data + vector->entrySize * index);
}

void vector_insert(Vector* vector, uint64_t index, void* entry)
{
    if (vector->length == vector->reservedLength)
    {
        vector_resize(vector, vector->reservedLength * 2);
    }

    memmove(vector_get(vector, index + 1), vector_get(vector, index), (vector->length - (index + 1)) * vector->entrySize);
    vector->length++;

    vector_set(vector, index, entry);
}

void vector_erase(Vector* vector, uint64_t index)
{
    memcpy(vector_get(vector, index), vector_get(vector, index + 1), (vector->length - (index + 1)) * vector->entrySize);
    vector->length--;
}

uint64_t vector_length(Vector* vector)
{
    return vector->length;
}