#include <ctype.h>

#include "common/ascii_table.h"

int isalnum(int c)
{
    return _AsciiTable[c].flags & (_ASCII_ALPHA | _ASCII_DIGIT);
}
