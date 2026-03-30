#include <stdlib.h>

div_t div(int numer, int denom)
{
    div_t rc;
    rc.quot = numer / denom;
    rc.rem = numer % denom;
    /* TODO: pre-C99 compilers might require modulus corrections */
    return rc;
}
