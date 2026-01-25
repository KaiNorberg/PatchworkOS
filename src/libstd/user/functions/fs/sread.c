#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

char* reads(fd_t fd)
{
    uint64_t size = 4096;
    char* buffer = malloc(size);
    if (buffer == NULL)
    {
        return NULL;
    }

    uint64_t totalRead = 0;
    while (1)
    {
        if (totalRead == size)
        {
            size *= 2;
            char* newBuffer = realloc(buffer, size);
            if (newBuffer == NULL)
            {
                free(buffer);
                return NULL;
            }
            buffer = newBuffer;
        }

        uint64_t bytesRead = read(fd, buffer + totalRead, size - totalRead);
        if (bytesRead == _FAIL)
        {
            free(buffer);
            return NULL;
        }

        if (bytesRead == 0)
        {
            break;
        }
        totalRead += bytesRead;
    }

    if (totalRead + 1 < size)
    {
        char* newBuffer = realloc(buffer, totalRead + 1);
        if (newBuffer != NULL)
        {
            buffer = newBuffer;
        }
    }

    buffer[totalRead] = '\0';
    return buffer;
}