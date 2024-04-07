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
    "Bed request"
};

_PUBLIC char* strerror(int error)
{
    if (error > EREQ || error < 0)
    {
        return "Unknown error";
    }
    else
    {
        return errorStrings[error];
    }
}