#include <ctype.h>

#include "common/ascii_table.h"

int isalnum(int c)
{
    return _ascii_table[c].flags & (_ASCII_ALPHA | _ASCII_DIGIT);
}
