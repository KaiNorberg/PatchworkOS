#include <stdio.h>

#include "platform/platform.h"
#include "platform/user/common/file.h"

int ungetc(int c, FILE* stream)
{
    int rc;

    _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);

    if (c == EOF || stream->ungetIndex == _UNGETC_MAX)
    {
        rc = -1;
    }
    else
    {
        rc = stream->ungetBuf[stream->ungetIndex++] = (unsigned char)c;
    }

    _PLATFORM_MUTEX_RELEASE(&stream->mtx);

    return rc;
}