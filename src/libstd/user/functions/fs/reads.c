#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

status_t reads(char** out, fd_t fd)
{
    uint64_t size = 4096;
    char* buffer = malloc(size);
    if (buffer == NULL)
    {
        return ERR(LIBSTD, NOMEM);
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
                return ERR(LIBSTD, NOMEM);
            }
            buffer = newBuffer;
        }

        uint64_t bytesRead;
        status_t status = read(fd, buffer + totalRead, size - totalRead, &bytesRead);
        if (IS_ERR(status))
        {
            free(buffer);
            return status;
        }

        totalRead += bytesRead;
        if (!IS_CODE(status, MORE))
        {
            break;
        }
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
    *out = buffer;
    return OK;
}