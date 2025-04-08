#include "platform/platform.h"
#if _PLATFORM_HAS_FILE_IO

#include <stdarg.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"

dir_list_t* allocdir(const char* path)
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

uint64_t open2(const char* path, fd_t fds[2])
{
    return _PlatformOpen2(path, fds);
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

uint64_t writef(fd_t fd, const char* _RESTRICT format, ...)
{
    typedef struct
    {
        fd_t fd;
        char buffer[MAX_PATH];
        uint64_t count;
    } writef_ctx_t;

    void put_func(char chr, void* context)
    {
        writef_ctx_t* ctx = (writef_ctx_t*)context;

        if (ctx->count >= MAX_PATH)
        {
            _PlatformWrite(ctx->fd, ctx->buffer, ctx->count);
            ctx->count = 0;
        }
        ctx->buffer[ctx->count++] = chr;
    }

    writef_ctx_t ctx = {
        .fd = fd,
        .count = 0,
    };

    va_list args;
    va_start(args, format);
    int result = _Print(put_func, &ctx, format, args);
    if (ctx.count > 0)
    {
        _PlatformWrite(fd, ctx.buffer, ctx.count);
    }
    va_end(args);
    return result;
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

poll_event_t poll1(fd_t fd, poll_event_t requested, nsec_t timeout)
{
    pollfd_t pollfd = {.fd = fd, .requested = requested};
    _PlatformPoll(&pollfd, 1, timeout);
    return pollfd.occurred;
}

uint64_t stat(const char* path, stat_t* info)
{
    return _PlatformStat(path, info);
}

uint64_t ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    return _PlatformIoctl(fd, request, argp, size);
}

uint64_t flush(fd_t fd, const pixel_t* buffer, uint64_t size, const rect_t* rect)
{
    return _PlatformFlush(fd, buffer, size, rect);
}

#endif
