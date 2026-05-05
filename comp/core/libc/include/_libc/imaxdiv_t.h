#ifndef _INTERNAL_IMAXDIV_T_H
#define _INTERNAL_IMAXDIV_T_H 1

#include "intmax_t.h"

typedef struct
{
    intmax_t quot;
    intmax_t rem;
} imaxdiv_t;

#endif
