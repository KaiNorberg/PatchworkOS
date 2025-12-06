#include <sys/io.h>
#include <stdlib.h>
#include <stdio.h>

char* sread(fd_t fd)
{
    uint64_t size = 128;
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
        if (bytesRead == ERR)
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

    buffer[totalRead] = '\0';
    return buffer;
}