#include <stdio.h>

#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

FILE* fopen(const char* _RESTRICT filename, const char* _RESTRICT mode)
{
    _FileFlags_t flags = _FileFlagsParse(mode);
    if (flags == 0)
    {
        return NULL;
    }

    if (flags & _FILE_APPEND)
    {
        // TODO: Implement append mode in kernel
        return NULL;
    }

    if (filename == NULL || filename[0] == '\0')
    {
        return NULL;
    }

    // TODO: Implement append mode in kernel
    fd_t fd = _SyscallOpen(filename);
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
