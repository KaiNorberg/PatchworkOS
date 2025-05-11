#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

#include "log.h"
#include "systime.h"

int vprintf(const char* _RESTRICT format, va_list args)
{
    char buffer[MAX_PATH];

    clock_t time = log_time_enabled() ? systime_uptime() : 0;
    clock_t sec = time / CLOCKS_PER_SEC;
    clock_t ms = (time % CLOCKS_PER_SEC) / (CLOCKS_PER_SEC / 1000);

    uint64_t result = vsprintf(buffer + sprintf(buffer, "[%10llu.%03llu] ", sec, ms), format, args);

    char newline[] = {'\n', '\0'};
    strcat(buffer, newline);
    log_print(buffer);
    return result + 1;
}
