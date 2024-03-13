#include <lib-asym.h>

const char* status_string(void)
{
    return statusToString[status()];
}