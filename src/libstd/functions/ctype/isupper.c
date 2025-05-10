#include <ctype.h>

#include "common/ascii_table.h"

int isupper(int c)
{
    return (_AsciiTable[c].flags & _ASCII_UPPER);
}
