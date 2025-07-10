#include <assert.h>
#include <errno.h>

#include "log/log.h"
#include "log/panic.h"

void _assert_99(const char* const message1, const char* const function, const char* const message2)
{
    panic(NULL, "%s %s %s %s", message1, function, message2, errno != 0 ? strerror(errno) : "errno not set");
}

void _assert_89(const char* const message)
{
    panic(NULL, message);
}
