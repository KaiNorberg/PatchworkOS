#include <errno.h>
#include <string.h>

#include "common/error_strings.h"

char* strerror(int errnum)
{
    if (errnum >= ERROR_MAX || errnum < 0)
    {
        return "unknown error";
    }
    else
    {
        return _ErrorStrings[errnum];
    }
}
