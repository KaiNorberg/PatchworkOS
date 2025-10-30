#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _KERNEL_
#include <kernel/log/panic.h>
#endif

void _assert_99(const char* const message1, const char* const function, const char* const message2)
{
#ifdef _KERNEL_
    panic(NULL, "%s %s %s %s", message1, function, message2, errno != 0 ? strerror(errno) : "errno not set");
#else
    fputs(message1, stderr);
    fputs(function, stderr);
    fputs(message2, stderr);
    abort();
#endif
}

void _assert_89(const char* const message)
{
#ifdef _KERNEL_
    panic(NULL, message);
#else
    fputs(message, stderr);
    abort();
#endif
}
