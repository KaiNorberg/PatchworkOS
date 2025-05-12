#include <stdio.h>

#include "platform/user/common/file.h"

void clearerr(FILE* stream)
{
    stream->flags &= ~(_FILE_ERROR | _FILE_EOF);
}
