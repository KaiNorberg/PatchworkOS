#include <ctype.h>

#include "common/ascii_table.h"

int islower(int c)
{
    return (_ascii_table[c].flags & _ASCII_LOWER);
}
