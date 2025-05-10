#include <ctype.h>

#include "common/ascii_table.h"

int isspace(int c)
{
    return (_AsciiTable[c].flags & _ASCII_SPACE);
}
