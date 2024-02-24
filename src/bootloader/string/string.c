#include "string.h"

#include <stdint.h>

#include "efilib.h"

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

size_t strlen(const char* str)
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
    for (uint64_t i = 0; i < len; i++)
    {
        dest[i] = src[i];
    }

    return dest;
}

int strcmp(const char* str1, const char* str2)
{
	int i = 0;
	while (str1[i] != 0 && str2[i] != 0)
	{
		if (str1[i] != str2[i])
		{
			return 0;
		}
		i++;
	}

	return (i != 0);
}

size_t strlen16(const CHAR16* str)
{
    CHAR16* strPtr = (CHAR16*)str;
    while (*strPtr != '\0')
    {
        strPtr++;
    }
    return strPtr - str;
}

CHAR16* strcpy16(CHAR16* dest, const CHAR16* src)
{
    uint64_t len = strlen16(src);
    for (uint64_t i = 0; i < len; i++)
    {
        dest[i] = src[i];
    }

    return dest;
}

void char16_to_char(CHAR16* string, char* out)
{
	uint64_t stringLength = StrLen(string);

	for (uint64_t i = 0; i < stringLength; i++)
	{
		out[i] = string[i];
	}
	out[stringLength] = 0;
}