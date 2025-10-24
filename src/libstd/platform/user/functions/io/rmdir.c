#include <sys/io.h>

#include "platform/user/common/syscalls.h"

uint64_t rmdir(const char* path)
{
    return removef("%s:dir", path);
}
