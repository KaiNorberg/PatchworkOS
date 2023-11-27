#include "libc/include/stdio.h"

#include "libc/utils/utils.h"

#ifdef KERNEL_LIB

#include "kernel/file_system/file_system.h"

FILE* fopen(const char* filename, const char* mode)
{
    return file_system_get(filename);
}

#else

FILE* fopen(const char* filename, const char* mode)
{
    return (FILE*)syscall_helper(SYS_OPEN, (uint64_t)filename, (uint64_t)mode, 0);
}

#endif