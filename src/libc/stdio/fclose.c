#include "libc/include/stdio.h"

#ifdef KERNEL_LIB

#include "kernel/file_system/file_system.h"

int fclose(FILE* stream)
{

}

#else

int fclose(FILE* stream)
{

}

#endif