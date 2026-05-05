#include <libc/io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

status_t ioscanv(fd_t fd, size_t count, size_t offset, uint32_t* matches, const char* format, va_list args)
{
    char buffer[MAX_PATH];
    char* ptr = buffer;
    if (count > sizeof(buffer))
    {
        ptr = malloc(count + 1);
        if (ptr == NULL)
        {
            return ERR(LIBSTD, NOMEM);
        }
    }

    size_t bytesRead = 0;
    status_t status = ioread(fd, IOBUF(ptr, count), offset, &bytesRead);
    if (IS_ERR(status))
    {
        if (ptr != buffer)
        {
            free(ptr);
        }
        return status;
    }

    ptr[bytesRead] = '\0';
    int ret = vsscanf(ptr, format, args);
    if (ret < 0)
    {
        if (ptr != buffer)
        {
            free(ptr);
        }
        return ERR(LIBSTD, ILSEQ);
    }

    if (ptr != buffer)
    {
        free(ptr);
    }

    if (matches != NULL)
    {
        *matches = ret;
    }

    return status;
}