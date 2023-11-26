#include "libc/include/stdio.h"

#include "libc/utils/utils.h"

FILE* fopen(const char* filename, const char* mode)
{
    return (FILE*)syscall_helper(SYS_OPEN, (uint64_t)filename, (uint64_t)mode, 0);
}