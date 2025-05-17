#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

#include "drivers/systime/systime.h"
#include "utils/log.h"

int vprintf(const char* _RESTRICT format, va_list args)
{
    char buffer[MAX_PATH];
    vsprintf(buffer, format, args);
    log_print(buffer);
    return 0;
}
