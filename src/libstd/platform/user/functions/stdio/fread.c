#include <stdio.h>
#include <stdlib.h>

#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

size_t fread(void* _RESTRICT ptr, size_t size, size_t nmemb, FILE* _RESTRICT stream)
{
    mtx_lock(&stream->mtx);

    uint64_t n = 0;

    if (_file_prepare_read(stream) != ERR)
    {
        for (; n < nmemb; n++)
        {
            // TODO: For better performance, read block-wise, not byte-wise.
            for (uint64_t i = 0; i < size; i++)
            {
                if (_FILE_CHECK_AVAIL(stream) == ERR)
                {
                    mtx_unlock(&stream->mtx);
                    return n;
                }

                ((uint8_t*)ptr)[n * size + i] = _FILE_GETC(stream);
            }
        }
    }

    mtx_unlock(&stream->mtx);
    return n;
}
