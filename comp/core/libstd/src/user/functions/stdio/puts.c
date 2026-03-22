#include <stdio.h>

#include "user/common/file.h"

int puts(const char* _RESTRICT s)
{
    mtx_lock(&stdout->mtx);

    if (_file_prepare_write(stdout) == EOF)
    {
        mtx_unlock(&stdout->mtx);
        return EOF;
    }

    while (*s != '\0')
    {
        stdout->buf[stdout->bufIndex++] = *s++;

        if (stdout->bufIndex == stdout->bufSize)
        {
            if (_file_flush_buffer(stdout) == EOF)
            {
                mtx_unlock(&stdout->mtx);
                return EOF;
            }
        }
    }

    stdout->buf[stdout->bufIndex++] = '\n';

    if ((stdout->bufIndex == stdout->bufSize) || (stdout->flags & (_FILE_LINE_BUFFERED | _FILE_UNBUFFERED)))
    {
        int result = _file_flush_buffer(stdout);
        mtx_unlock(&stdout->mtx);
        return result;
    }

    mtx_unlock(&stdout->mtx);
    return 0;
}
