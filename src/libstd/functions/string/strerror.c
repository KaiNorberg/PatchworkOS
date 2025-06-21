#include <errno.h>
#include <string.h>

#include "common/error_strings.h"

char* strerror(int errnum)
{
    if (errnum >= ERR_MAX || errnum < 0)
    {
        return "unknown error";
    }
    else
    {
        return _error_strings[errnum];
    }
}
