#include <stdio.h>
#include <stdlib.h>

#include "user/common/file.h"

int putc(int c, FILE* stream)
{
    return fputc(c, stream);
}
