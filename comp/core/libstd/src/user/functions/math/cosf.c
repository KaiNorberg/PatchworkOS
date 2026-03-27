#include <errno.h>
#include <libstd/defs.h>
#include <math.h>
#include <stdbool.h>

float cosf(float x)
{
    if (x == 0.0F || x == -0.0F)
    {
        return 1.0F;
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
    ASM("fcos" : "=t"(result) : "0"(x));
    return result;
}
