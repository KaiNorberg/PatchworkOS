#include <libc/dstr.h>
#include <libc/status.h>
#include <stdlib.h>
#include <string.h>

status_t dstr_append(dstr_t* dstr, const char* data, size_t length)
{
    size_t newLength = dstr->length + length;
    if (newLength > DSTR_LENGTH_MAX)
    {
        return ERR(LIBSTD, INVAL);
    }

    if (newLength > dstr->capacity)
    {
        size_t newCapacity = dstr->capacity;
        while (newCapacity < newLength + 1)
        {
            newCapacity *= 2;
        }

        if (dstr->data == dstr->small)
        {
            char* newData = malloc(newCapacity);
            if (newData == NULL)
            {
                return ERR(LIBSTD, NOMEM);
            }
            memcpy(newData, dstr->small, dstr->length);
            dstr->data = newData;
        }
        else
        {
            char* newData = realloc(dstr->data, newCapacity);
            if (newData == NULL)
            {
                return ERR(LIBSTD, NOMEM);
            }
            dstr->data = newData;
        }
        dstr->capacity = newCapacity;
    }

    memcpy(dstr->data + dstr->length, data, length);
    dstr->length = newLength;

    return OK;
}