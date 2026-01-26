#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

status_t readdir(fd_t fd, dirent_t** buffer, uint64_t* count)
{
    uint64_t size = 1024 * sizeof(dirent_t);
    dirent_t* dirents = malloc(size);
    if (dirents == NULL)
    {
        return ERR(LIBSTD, NOMEM);
    }

    uint64_t totalRead = 0;
    while (1)
    {
        if (size - totalRead < sizeof(dirent_t))
        {
            size *= 2;
            dirent_t* newDirents = realloc(dirents, size);
            if (newDirents == NULL)
            {
                free(dirents);
                return ERR(LIBSTD, NOMEM);
            }
            dirents = newDirents;
        }

        size_t bytesRead;
        status_t status = getdents(fd, (dirent_t*)((uint8_t*)dirents + totalRead), size - totalRead, &bytesRead);
        if (IS_ERR(status))
        {
            free(dirents);
            return status;
        }

        if (bytesRead == 0)
        {
            break;
        }
        totalRead += bytesRead;
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
    return OK;
}