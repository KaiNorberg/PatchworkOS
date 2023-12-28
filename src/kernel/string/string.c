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

void* memcpy(void* dest, const void* src, const uint64_t count)
{	
    void* dstPtr = dest;
	void* srcPtr = (void*)src;

    for (uint64_t i = 0; i < count / sizeof(uint64_t); i++)
    {
        ((uint64_t*)dstPtr)[i] = ((uint64_t*)srcPtr)[i];
    }

    for (uint64_t i = 0; i < count % sizeof(uint64_t); i++)
    {
        ((uint8_t*)dstPtr)[count / sizeof(uint64_t) + i] = ((uint8_t*)srcPtr)[count / sizeof(uint64_t) + i];
    }

    return dstPtr;
}

void* memmove(void* dest, const void* src, uint64_t count)
{
    for (int i = 0; i < count / 16; i++)
    {
        asm volatile ("movups (%0), %%xmm0\n" "movntdq %%xmm0, (%1)\n" : : "r" (src), "r" (dest) : "memory");

        src += 16;
        dest += 16;
    }

    if (count & 7)
    {
        count = count & 7;

        int d0, d1, d2;
        asm volatile(
        "rep ; movsl\n\t"
        "testb $2,%b4\n\t"
        "je 1f\n\t"
        "movsw\n"
        "1:\ttestb $1,%b4\n\t"
        "je 2f\n\t"
        "movsb\n"
        "2:"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        :"0" (count/4), "q" (count),"1" ((long) dest),"2" ((long) src)
        : "memory");
    }
    return (dest);
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


