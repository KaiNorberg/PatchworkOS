#include <stdio.h>
#include <stdlib.h>

#include "user/common/file.h"

int putchar(int c)
{
    return fputc(c, stdout);
}
