#include <libstd/io.h>
#include <stdlib.h>

#define IOLOAD_CHUNK_SIZE 4096

status_t ioload(fd_t fd, char** out, size_t* outLen)
{
    if (out == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    size_t capacity = IOLOAD_CHUNK_SIZE;
    size_t size = 0;
    char* buffer = malloc(capacity);
    if (buffer == NULL)
    {
        return ERR(LIBSTD, NOMEM);
    }

    status_t status;
    while (true)
    {
        if (size == capacity)
        {
            size_t newCapacity = capacity * 2;
            char* newBuffer = realloc(buffer, newCapacity);
            if (newBuffer == NULL)
            {
                free(buffer);
                return ERR(LIBSTD, NOMEM);
            }
            buffer = newBuffer;
            capacity = newCapacity;
        }

        size_t bytesRead = 0;
        status = ioread(fd, IOBUF(buffer + size, capacity - size), IOCUR, &bytesRead);

        if (IS_ERR(status))
        {
            free(buffer);
            return status;
        }

        size += bytesRead;

        if (bytesRead == 0 || IS_CODE(status, EOF))
        {
            break;
        }
    }

    char* newBuffer = realloc(buffer, size + 1);
    if (newBuffer != NULL)
    {
        buffer = newBuffer;
    }
    buffer[size] = '\0';

    *out = buffer;
    if (outLen != NULL)
    {
        *outLen = size;
    }

    return OK;
}