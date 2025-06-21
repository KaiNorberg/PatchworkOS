#include <ctype.h>

#include "common/ascii_table.h"

int ispunct(int c)
{
    return (_ascii_table[c].flags & _ASCII_PUNCT);
}
