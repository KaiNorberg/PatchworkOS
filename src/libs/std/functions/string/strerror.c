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
    "Invalid name",
    "To many open files",
    "Bad file descriptor",
    "Permission denied",
    "Bad executable",
    "Out of memory"
};

_EXPORT char* strerror(int error)
{
    if (error > ENOMEM || error < 0)
    {
        return "Unknown error";
    }
    else
    {
        return errorStrings[error];
    }
}