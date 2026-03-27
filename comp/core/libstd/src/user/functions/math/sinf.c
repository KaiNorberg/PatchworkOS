#include <errno.h>
#include <libstd/defs.h>
#include <math.h>
#include <stdbool.h>

float sinf(float x)
{
    if (x == 0.0F || x == -0.0F)
    {
        return 0.0F;
    }

    if (isinf(x))
    {
        errno = EDOM;
        return NAN;
    }

    if (isnan(x))
    {
        return NAN;
    }

    float result;
    ASM("fsin" : "=t"(result) : "0"(x));
    return result;
}
