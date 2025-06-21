#include <ctype.h>

#include "common/ascii_table.h"

int iscntrl(int c)
{
    return (_ascii_table[c].flags & _ASCII_CNTRL);
}
