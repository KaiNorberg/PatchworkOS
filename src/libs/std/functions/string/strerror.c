#include <string.h>
#include <errno.h>

static char* errorStrings[] =
{
    "No error",
    "Math argument out of domain",
    "Math result not representable",
    "Illegal byte sequence",
    "Not implemented",
    "Bad address",
    "Already exists",
    "Invalid letter",
    "Invalid path",
    "To many open files",
    "Bad file descriptor",
    "Permission denied",
    "Bad executable",
    "Out of memory",
    "Bed request",
    "Bad flag/flags",
    "Invalid argument",
    "Bad buffer",
    "Busy"
};

_PUBLIC char* strerror(int error)
{
    if (error > EBUSY || error < 0)
    {
        return "Unknown error";
    }

    return errorStrings[error];
}