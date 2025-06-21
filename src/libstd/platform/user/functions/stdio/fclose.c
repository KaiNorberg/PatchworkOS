#include <stdio.h>
#include <stdlib.h>

#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

int fclose(struct FILE* stream)
{
    _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);

    if (stream->flags & _FILE_WRITE)
    {
        if (_file_flush_buffer(stream) == ERR)
        {
            _PLATFORM_MUTEX_RELEASE(&stream->mtx);
            return EOF;
        }
    }

    _PLATFORM_MUTEX_RELEASE(&stream->mtx);

    _files_remove(stream);
    _file_deinit(stream);
    _file_free(stream);
    return 0;
}
