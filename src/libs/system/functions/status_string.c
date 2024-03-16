#include <lib-system.h>

const char* status_string(void)
{
    return statusToString[status()];
}