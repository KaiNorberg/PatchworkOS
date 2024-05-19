#include "char16.h"

#include <stdint.h>

#include "efilib.h"

void char16_to_char(CHAR16* string, char* out)
{
    uint64_t stringLength = StrLen(string);

    for (uint64_t i = 0; i < stringLength; i++)
	{
	    out[i] = string[i];
	}
    out[stringLength] = 0;
}