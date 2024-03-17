#include <lib-system.h>

static const char* statusToString[] =
{
    "SUCCESS",
    "FAILURE",
    "INVALID_NAME",
    "INVALID_PATH",
    "ALREADY_EXISTS",
    "NOT_ALLOWED",
    "END_OF_FILE",
    "CORRUPT",
    "INVALID_POINTER",
    "INVALID_FLAG",
    "DOES_NOT_EXIST",
    "INSUFFICIENT_SPACE"
};

const char* status_string(uint64_t status)
{
    return statusToString[status];
}