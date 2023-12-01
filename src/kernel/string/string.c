#include "string.h"

int memcmp(const void* lhs, const void* rhs, size_t count)
{
	const unsigned char* a = (const unsigned char*) lhs;
	const unsigned char* b = (const unsigned char*) rhs;
	for (size_t i = 0; i < count; i++) 
    {
		if (a[i] < b[i])
        {
            return -1;
        }
		else if (b[i] < a[i])
        {
            return 1;
        }
	}
	return 0;
}

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

void* memmove(void* dest, const void* src, size_t count)
{
    unsigned char* dstPtr = (unsigned char*)dest;
	const unsigned char* srcPtr = (const unsigned char*)src;
	if (dstPtr < srcPtr) 
    {
		for (size_t i = 0; i < count; i++)
        {
            dstPtr[i] = srcPtr[i];
        }

	} 
    else 
    {		
		for (size_t i = count; i != 0; i--)
        {
            dstPtr[i-1] = srcPtr[i-1];
        }
	}
	return dstPtr;
}

void* memset(void *dest, int ch, size_t count)
{
    unsigned char* dstPtr = (unsigned char*)dest;
	for (size_t i = 0; i < count; i++)
    {
        dstPtr[i] = (unsigned char) ch;
    }
	return dstPtr;
}

size_t strlen(const char* str)
{
    char* strPtr = (char*)str;
    while (*strPtr != '\0')
    {
        strPtr++;
    }
    return strPtr - str;
}