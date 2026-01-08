#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "user/common/syscalls.h"

size_t readdir(fd_t fd, dirent_t** buffer, uint64_t* count)
{
    uint64_t size = 1024 * sizeof(dirent_t);
    dirent_t* dirents = malloc(size);
    if (dirents == NULL)
    {
        return ERR;
    }

    uint64_t totalRead = 0;
    while (1)
    {
        uint64_t bytesRead = getdents(fd, (dirent_t*)((uint64_t)dirents + totalRead), size - totalRead);
        if (bytesRead == ERR)
        {
            free(dirents);
            return ERR;
        }

        if (bytesRead == 0)
        {
            break;
        }
        totalRead += bytesRead;

        if (size - totalRead < sizeof(dirent_t))
        {
            size *= 2;
            dirent_t* newDirents = realloc(dirents, size);
            if (newDirents == NULL)
            {
                free(dirents);
                return ERR;
            }
            dirents = newDirents;
        }
    }

    if (totalRead > 0 && totalRead < size)
    {
        dirent_t* newDirents = realloc(dirents, totalRead);
        if (newDirents != NULL)
        {
            dirents = newDirents;
        }
    }

    *buffer = dirents;
    *count = totalRead / sizeof(dirent_t);
    return 0;
}