#include <stdio.h>

#include "platform/user/common/file.h"

int puts(const char* _RESTRICT s)
{
    _PLATFORM_MUTEX_ACQUIRE(&stdout->mtx);

    if (_file_prepare_write(stdout) == ERR)
    {
        _PLATFORM_MUTEX_RELEASE(&stdout->mtx);
        return EOF;
    }

    while (*s != '\0')
    {
        stdout->buf[stdout->bufIndex++] = *s++;

        if (stdout->bufIndex == stdout->bufSize)
        {
            if (_file_flush_buffer(stdout) == ERR)
            {
                _PLATFORM_MUTEX_RELEASE(&stdout->mtx);
                return EOF;
            }
        }
    }

    stdout->buf[stdout->bufIndex++] = '\n';

    if ((stdout->bufIndex == stdout->bufSize) || (stdout->flags & (_FILE_LINE_BUFFERED | _FILE_UNBUFFERED)))
    {
        uint64_t result = _file_flush_buffer(stdout);
        _PLATFORM_MUTEX_RELEASE(&stdout->mtx);
        return result == ERR ? EOF : 0;
    }
    else
    {
        _PLATFORM_MUTEX_RELEASE(&stdout->mtx);
        return 0;
    }
}
