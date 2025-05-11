#include <stdio.h>
#include <stdlib.h>

#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

int putc(int c, FILE* stream)
{
    return fputc(c, stream);
}
