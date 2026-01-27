#include <stdio.h>
#include <stdlib.h>

#include "user/common/file.h"
#include "user/common/syscalls.h"

size_t fread(void* _RESTRICT ptr, size_t size, size_t nmemb, FILE* _RESTRICT stream)
{
    mtx_lock(&stream->mtx);

    uint64_t n = 0;

    if (_file_prepare_read(stream) != EOF)
    {
        for (; n < nmemb; n++)
        {
            for (uint64_t i = 0; i < size; i++)
            {
                if (_FILE_CHECK_AVAIL(stream) == EOF)
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
