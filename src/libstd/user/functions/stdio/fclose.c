#include <stdio.h>
#include <stdlib.h>

#include "user/common/file.h"
#include "user/common/syscalls.h"

int fclose(struct FILE* stream)
{
    mtx_lock(&stream->mtx);

    if (stream->flags & _FILE_WRITE)
    {
        if (_file_flush_buffer(stream) == ERR)
        {
            mtx_unlock(&stream->mtx);
            return EOF;
        }
    }

    mtx_unlock(&stream->mtx);

    _files_remove(stream);
    _file_deinit(stream);
    _file_free(stream);
    return 0;
}
