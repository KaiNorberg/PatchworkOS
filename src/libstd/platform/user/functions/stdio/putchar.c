#include <stdio.h>
#include <stdlib.h>

#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

int putchar(int c)
{
    return fputc(c, stdout);
}
