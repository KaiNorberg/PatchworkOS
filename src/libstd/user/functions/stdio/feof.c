#include <stdio.h>

#include "user/common/file.h"

int feof(FILE* stream)
{
    return stream->flags & _FILE_EOF;
}
