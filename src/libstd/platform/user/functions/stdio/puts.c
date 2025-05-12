#include <stdio.h>

#include "platform/user/common/file.h"

int puts(const char* _RESTRICT s)
{
    return fputs(s, stdout);
}
