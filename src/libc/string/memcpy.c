#include "libc/include/string.h"

void* memcpy(void* dest, const void* src, size_t count)
{	
    unsigned char* dstPtr = (unsigned char*)dest;
	const unsigned char* srcPtr = (const unsigned char*)src;
    for (size_t i = 0; i < count; i++)
    {
        dstPtr[i] = srcPtr[i];
    }
    return dstPtr;
}

