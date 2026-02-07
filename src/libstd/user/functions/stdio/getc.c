#include <stdio.h>
#include <stdlib.h>

#include "user/common/file.h"

int getc(FILE* stream)
{
    return fgetc(stream);
}
