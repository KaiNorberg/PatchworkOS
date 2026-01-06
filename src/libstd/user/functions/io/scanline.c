#include <sys/io.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/proc.h>

uint64_t scanline(fd_t fd, const char* format, ...)
{
    char buffer[PAGE_SIZE];
    uint64_t result = readline(fd, buffer, sizeof(buffer));
    if (result == ERR)
    {
        return ERR;
    }

    if (result == 0)
    {
        return 0;
    }

    printf("scanline: read line '%s'\n", buffer);
    va_list args;
    va_start(args, format);
    int items = vsscanf(buffer, format, args);
    va_end(args);

    return items < 0 ? ERR : (uint64_t)items;
}