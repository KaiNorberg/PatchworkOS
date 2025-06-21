#include <ctype.h>

#include "common/ascii_table.h"

int isprint(int c)
{
    /* FIXME: Space as of current locale charset, not source charset. */
    return (_ascii_table[c].flags & _ASCII_GRAPH) || (c == ' ');
}
