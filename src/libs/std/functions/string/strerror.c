#include <errno.h>
#include <string.h>

static char* errorStrings[] = {
    "no error",
    "math argument out of domain",
    "math result not representable",
    "illegal byte sequence",
    "not implemented",
    "bad address",
    "already exists",
    "invalid letter",
    "invalid path",
    "too many open files",
    "bad file descriptor",
    "permission denied",
    "bad executable",
    "out of memory",
    "bad request",
    "bad flag/flags",
    "invalid argument",
    "bad buffer",
    "not a directory",
    "is a directory",
    "no such resource",
    "busy",
};

char* strerror(int error)
{
    if (error > EBUSY || error < 0)
    {
        return "Unknown error";
    }

    return errorStrings[error];
}