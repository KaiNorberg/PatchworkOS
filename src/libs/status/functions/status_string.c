#include <lib-status.h>

const char* status_string()
{
    return statusToString[status()];
}