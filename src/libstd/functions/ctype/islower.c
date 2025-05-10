#include <ctype.h>

#include "common/ascii_table.h"

int islower(int c)
{
    return (_AsciiTable[c].flags & _ASCII_LOWER);
}
