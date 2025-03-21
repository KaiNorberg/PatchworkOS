#include "platform/platform.h"
#if _PLATFORM_HAS_FILE_IO

#include <stdlib.h>
#include <sys/io.h>

#include "platform/platform.h"

dir_list_t* loaddir(const char* path)
{
    uint64_t amount = listdir(path, NULL, 0);
    if (amount == ERR)
    {
        return NULL;
    }

    dir_list_t* list = malloc(sizeof(dir_list_t) + sizeof(dir_entry_t) * amount);
    if (list == NULL)
    {
        return NULL;
    }

    list->amount = amount;
    if (listdir(path, list->entries, amount) == ERR)
    {
        free(list);
        return NULL;
    }

    return list;
}

uint64_t listdir(const char* path, dir_entry_t* entries, uint64_t amount)
{
    return _PlatformListdir(path, entries, amount);
}

fd_t open(const char* path)
{
    return _PlatformOpen(path);
}

uint64_t close(fd_t fd)
{
    return _PlatformClose(fd);
}

uint64_t read(fd_t fd, void* buffer, uint64_t count)
{
    return _PlatformRead(fd, buffer, count);
}

uint64_t write(fd_t fd, const void* buffer, uint64_t count)
{
    return _PlatformWrite(fd, buffer, count);
}

uint64_t seek(fd_t fd, int64_t offset, seek_origin_t origin)
{
    return _PlatformSeek(fd, offset, origin);
}

uint64_t realpath(char* out, const char* path)
{
    return _PlatformRealpath(out, path);
}

uint64_t chdir(const char* path)
{
    return _PlatformChdir(path);
}

uint64_t poll(pollfd_t* fds, uint64_t amount, nsec_t timeout)
{
    return _PlatformPoll(fds, amount, timeout);
}

uint64_t stat(const char* path, stat_t* stat)
{
    return _PlatformStat(path, stat);
}

uint64_t ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    return _PlatformIoctl(fd, request, argp, size);
}

uint64_t flush(fd_t fd, const pixel_t* buffer, uint64_t size, const rect_t* rect)
{
    return _PlatformFlush(fd, buffer, size, rect);
}

uint64_t pipe(pipefd_t* pipefd)
{
    return _PlatformPipe(pipefd);
}

#endif
