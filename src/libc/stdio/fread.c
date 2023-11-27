#include "libc/include/stdio.h"

#ifdef KERNEL_LIB

#include "kernel/file_system/file_system.h"

size_t fread(void* buffer, size_t size, size_t count, FILE* stream)
{
    
}

#else

size_t fread(void* buffer, size_t size, size_t count, FILE* stream)
{
    
}

#endif
