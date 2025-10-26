#include <stdio.h>
#include <stdlib.h>

#include "user/common/file.h"
#include "user/common/syscalls.h"

int putc(int c, FILE* stream)
{
    return fputc(c, stream);
}
