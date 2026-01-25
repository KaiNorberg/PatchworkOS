#include <stdio.h>

#include "user/common/file.h"

int fflush(FILE* stream)
{
    uint64_t result = 0;
    if (stream == NULL)
    {
        result = _files_flush();
    }
    else
    {
        mtx_lock(&stream->mtx);
        result = _file_flush_buffer(stream);
        mtx_unlock(&stream->mtx);
    }

    return result == _FAIL ? EOF : 0;
}
