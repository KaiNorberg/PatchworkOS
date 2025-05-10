#include <ctype.h>

#include "common/ascii_table.h"

int isxdigit(int c)
{
    return _AsciiTable[c].flags & _ASCII_XDIGIT;
}
