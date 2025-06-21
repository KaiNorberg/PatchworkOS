#include <ctype.h>

#include "common/ascii_table.h"

int toupper(int c)
{
    return _ascii_table[c].upper;
}
