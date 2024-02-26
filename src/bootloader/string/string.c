#include "string.h"

#include <stdint.h>

#include "efilib.h"

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

void char16_to_char(CHAR16* string, char* out)
{
	uint64_t stringLength = StrLen(string);

	for (uint64_t i = 0; i < stringLength; i++)
	{
		out[i] = string[i];
	}
	out[stringLength] = 0;
}