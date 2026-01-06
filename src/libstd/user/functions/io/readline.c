#include <sys/io.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

uint64_t readline(fd_t fd, char* buffer, uint64_t size)
{
    if (buffer == NULL || size == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t totalRead = 0;
    while (totalRead < size - 1)
    {
        char ch;
        uint64_t bytesRead = read(fd, &ch, 1);
        if (bytesRead == ERR)
        {
            return ERR;
        }
        
        if (bytesRead == 0) // EOF
        {
            break;
        }

        if (ch == '\n')
        {
            break;
        }

        buffer[totalRead++] = ch;
    }

    buffer[totalRead] = '\0';
    return totalRead;
}