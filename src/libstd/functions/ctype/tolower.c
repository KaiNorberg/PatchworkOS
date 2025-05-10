#include <ctype.h>

#include "common/ascii_table.h"

int tolower(int c)
{
    return _AsciiTable[c].lower;
}
