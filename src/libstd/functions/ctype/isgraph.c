#include <ctype.h>

#include "common/ascii_table.h"

int isgraph(int c)
{
    return (_AsciiTable[c].flags & _ASCII_GRAPH);
}
