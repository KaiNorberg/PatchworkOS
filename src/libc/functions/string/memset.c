#include <stdint.h>
#include <stddef.h>

void* memset(void *dest, int ch, size_t count)
{
    unsigned char* dstPtr = (unsigned char*)dest;
	for (size_t i = 0; i < count; i++)
    {
        dstPtr[i] = (unsigned char) ch;
    }
	return dstPtr;
}