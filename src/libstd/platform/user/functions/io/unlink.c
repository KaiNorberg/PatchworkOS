#include <sys/io.h>

#include "platform/user/common/syscalls.h"

uint64_t unlink(const char* path)
{
    return delete(path);
}
