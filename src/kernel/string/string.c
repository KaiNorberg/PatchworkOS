#include "string.h"

int memcmp(const void* lhs, const void* rhs, uint64_t count)
{
	const unsigned char* a = (const unsigned char*) lhs;
	const unsigned char* b = (const unsigned char*) rhs;
	for (uint64_t i = 0; i < count; i++) 
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

void* memcpy(void* dest, const void* src, uint64_t count)
{	
    unsigned char* dstPtr = (unsigned char*)dest;
	const unsigned char* srcPtr = (const unsigned char*)src;
    for (uint64_t i = 0; i < count; i++)
    {
        dstPtr[i] = srcPtr[i];
    }
    return dstPtr;
}

void* memmove(void* dest, const void* src, uint64_t count)
{
    unsigned char* dstPtr = (unsigned char*)dest;
	const unsigned char* srcPtr = (const unsigned char*)src;
	if (dstPtr < srcPtr) 
    {
		for (uint64_t i = 0; i < count; i++)
        {
            dstPtr[i] = srcPtr[i];
        }

	} 
    else 
    {		
		for (uint64_t i = count; i != 0; i--)
        {
            dstPtr[i-1] = srcPtr[i-1];
        }
	}
	return dstPtr;
}

void* memset(void *dest, int ch, uint64_t count)
{
    unsigned char* dstPtr = (unsigned char*)dest;
	for (uint64_t i = 0; i < count; i++)
    {
        dstPtr[i] = (unsigned char) ch;
    }
	return dstPtr;
}

void* memclr(void* start, uint64_t count)
{
    asm("rep stosl;"::"a"(0),"D"((uint64_t)start),"c"(count / 4));
    asm("rep stosb;"::"a"(0),"D"(((uint64_t)start) + ((count / 4) * 4)),"c"(count - ((count / 4) * 4)));

    return (void *)((uint64_t)start + count);
}

uint64_t strlen(const char* str)
{
    char* strPtr = (char*)str;
    while (*strPtr != '\0')
    {
        strPtr++;
    }
    return strPtr - str;
}

char* strcpy(char* dest, const char* src)
{
    uint64_t len = strlen(src);
    for (int i = 0; i < len; i++)
    {
        dest[i] = src[i];
    }

    return dest;
}


