#include <lib-asym.h>

const char* status_string()
{
    return statusToString[status()];
}