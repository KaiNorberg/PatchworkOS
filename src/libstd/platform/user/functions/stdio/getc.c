#include <stdio.h>
#include <stdlib.h>

#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

int getc(FILE* stream)
{
    return fgetc(stream);
}
