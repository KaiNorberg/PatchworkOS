#include <stdio.h>

#include "user/common/file.h"

int ferror(FILE* stream)
{
    return stream->flags & _FILE_ERROR;
}
