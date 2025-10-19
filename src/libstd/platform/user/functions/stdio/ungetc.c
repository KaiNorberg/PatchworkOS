#include <stdio.h>

#include "platform/platform.h"
#include "platform/user/common/file.h"

int ungetc(int c, FILE* stream)
{
    int rc;

    mtx_lock(&stream->mtx);

    if (c == EOF || stream->ungetIndex == _UNGETC_MAX)
    {
        rc = -1;
    }
    else
    {
        rc = stream->ungetBuf[stream->ungetIndex++] = (unsigned char)c;
    }

    mtx_unlock(&stream->mtx);

    return rc;
}
