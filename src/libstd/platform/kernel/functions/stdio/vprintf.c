#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

#include "log.h"
#include "systime.h"

int vprintf(const char* _RESTRICT format, va_list args)
{
    char buffer[MAX_PATH];
    vsprintf(buffer, format, args);
    log_print(buffer);
    return 0;
}
