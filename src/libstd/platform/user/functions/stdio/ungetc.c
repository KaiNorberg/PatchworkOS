#include <stdio.h>

#include "platform/platform.h"
#include "platform/user/common/file.h"

int ungetc(int c, FILE* stream)
{
    _PLATFORM_MUTEX_ACQUIRE(&stream->mtx);

    int result = _FileUngetcUnlocked(stream, c);

    _PLATFORM_MUTEX_RELEASE(&stream->mtx);

    return result;
}