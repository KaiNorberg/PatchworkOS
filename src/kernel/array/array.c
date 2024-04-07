#include "array.h"

#include <string.h>

#include "heap/heap.h"

static void array_erase_unlocked(Array* array, uint64_t index)
{
    for (uint64_t i = index; i < array->length - 1; i++)
    {
        array->data[i] = array->data[i + 1];
    }
    array->length--;
}

Array* array_new()
{
    Array* array = kmalloc(sizeof(Array));
    array->data = kcalloc(ARRAY_INIT_CAPACITY, sizeof(void*));
    array->capacity = ARRAY_INIT_CAPACITY;
    array->length = 0;
    array->lock = lock_create();

    return array;
}

void array_free(Array* array)
{
    kfree(array->data);
    kfree(array);
}

void array_push(Array* array, void* element)
{
    LOCK_GUARD(array->lock);

    if (array->length == array->capacity)
    {
        array->capacity = array->capacity * 2;

        void* newData = kcalloc(array->capacity, sizeof(void*));
        memcpy(newData, array->data, array->length * sizeof(void*));

        kfree(array->data);
        array->data = newData;
    }

    array->data[array->length] = element;
    array->length++;
}

void* array_find(Array* array, FindResult(*callback)(void*, void*), void* context)
{
    LOCK_GUARD(array->lock);

    for (int64_t i = 0; i < (int64_t)array->length; i++)
    {
        uint64_t result = callback(array->data[i], context);

        if (result == FIND_FOUND)
        {
            return array->data[i];
        }
    }

    return NULL;
}

bool array_iterate(Array* array, IterResult(*callback)(void*))
{
    LOCK_GUARD(array->lock);

    for (int64_t i = 0; i < (int64_t)array->length; i++)
    {
        uint64_t result = callback(array->data[i]);

        if (result == ITERATE_BREAK)
        {
            return false;
        }
        else if (result == ITERATE_ERASE)
        {
            array_erase_unlocked(array, i);
            i--;
        }
    }

    return true;
}

uint64_t array_length(Array* array)
{
    lock_acquire(&array->lock);
    uint64_t temp = array->length;
    lock_release(&array->lock);
    return temp;
}