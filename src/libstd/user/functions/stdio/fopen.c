#include <errno.h>
#include <stdio.h>
#include <sys/fs.h>

#include "user/common/file.h"
#include "user/common/syscalls.h"

static const char* _flags_to_string(_file_flags_t flags)
{
    switch (flags & (_FILE_READ | _FILE_WRITE | _FILE_APPEND | _FILE_RW))
    {
    case _FILE_READ:
        return "";
    case _FILE_WRITE:
        return ":create:truncate";
    case _FILE_APPEND:
        return ":append:create";
    case _FILE_READ | _FILE_RW:
        return "";
    case _FILE_WRITE | _FILE_RW:
        return ":truncate:create";
    case _FILE_APPEND | _FILE_RW:
        return ":append:create";
    default:
        return "";
    }
}

FILE* fopen(const char* _RESTRICT filename, const char* _RESTRICT mode)
{
    _file_flags_t flags = _file_flags_parse(mode);
    if (flags == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    if (filename == NULL || filename[0] == '\0')
    {
        errno = EINVAL;
        return NULL;
    }

    fd_t fd;
    status_t status = open(&fd, F("%s%s", filename, _flags_to_string(flags)));
    if (IS_ERR(status))
    {
        errno = ENOENT;
        return NULL;
    }

    FILE* stream = _file_new();
    if (stream == NULL)
    {
        errno = ENOMEM;
        close(fd);
        return NULL;
    }

    if (_file_init(stream, fd, flags | _FILE_FULLY_BUFFERED, NULL, BUFSIZ) == EOF)
    {
        errno = ENOMEM;
        close(fd);
        _file_free(stream);
        return NULL;
    }

    _files_push(stream);
    return stream;
}
