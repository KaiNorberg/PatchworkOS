#include "libc/auxiliary/system/system.h"

void exit(int code)
{
    system_exit(code);
}