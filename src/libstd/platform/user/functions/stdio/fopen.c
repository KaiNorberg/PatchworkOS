#include <stdio.h>

#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

static const char* _FlagsToString(_FileFlags_t flags)
{
    switch (flags & (_FILE_READ | _FILE_WRITE | _FILE_APPEND | _FILE_RW))
    {
    case _FILE_READ:
        return "";
    case _FILE_WRITE:
        return "?create&trunc";
    case _FILE_APPEND:
        return "?append&create";
    case _FILE_READ | _FILE_RW:
        return "";
    case _FILE_WRITE | _FILE_RW:
        return "?trunc&create";
    case _FILE_APPEND | _FILE_RW:
        return "?append&create";
    default:
        return "";
    }
}

FILE* fopen(const char* _RESTRICT filename, const char* _RESTRICT mode)
{
    _FileFlags_t flags = _FileFlagsParse(mode);
    if (flags == 0)
    {
        return NULL;
    }

    if (filename == NULL || filename[0] == '\0')
    {
        return NULL;
    }

    fd_t fd = openf("%s%s", filename, _FlagsToString(flags));
    if (fd == ERR)
    {
        return NULL;
    }

    FILE* stream = _FileNew();
    if (stream == NULL)
    {
        return NULL;
    }

    if (_FileInit(stream, fd, flags | _FILE_FULLY_BUFFERED, NULL, BUFSIZ) == ERR)
    {
        close(fd);
        _FileFree(stream);
        return NULL;
    }

    _FilesPush(stream);
    return stream;
}
