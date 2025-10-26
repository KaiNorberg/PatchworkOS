#include <sys/io.h>

#include "user/common/syscalls.h"

uint64_t rmdir(const char* path)
{
    return removef("%s:dir", path);
}
