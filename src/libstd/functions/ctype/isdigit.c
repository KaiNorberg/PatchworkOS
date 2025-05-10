#include <ctype.h>

#include "common/ascii_table.h"

int isdigit(int c)
{
    return _AsciiTable[c].flags & _ASCII_DIGIT;
}
