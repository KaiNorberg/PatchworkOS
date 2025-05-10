#include <ctype.h>

#include "common/ascii_table.h"

int ispunct(int c)
{
    return (_AsciiTable[c].flags & _ASCII_PUNCT);
}
