#include <ctype.h>

#include "common/ascii_table.h"

int isblank(int c)
{
    return (_AsciiTable[c].flags & _ASCII_BLANK);
}
