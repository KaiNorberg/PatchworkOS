#include <stdio.h>
#include <stdlib.h>

#include "user/common/file.h"
#include "user/common/syscalls.h"

int putchar(int c)
{
    return fputc(c, stdout);
}
