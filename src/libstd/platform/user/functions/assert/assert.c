#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void _assert_99(const char* const message1, const char* const function, const char* const message2)
{
    fputs(message1, stderr);
    fputs(function, stderr);
    fputs(message2, stderr);
    abort();
}

void _assert_89(const char* const message)
{
    fputs(message, stderr);
    abort();
}
