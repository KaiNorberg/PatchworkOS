#include <math.h>

int ilogb(double x)
{
    if (x == 0.0)
    {
        return FP_ILOGB0;
    }
    if (isnan(x) || isinf(x))
    {
        return FP_ILOGBNAN;
    }

    _double_t d = {.val = x};
    if (d.exp == 0)
    {
        _double_t d2 = {.val = x * _DOUBLE_MAX_INTEGER};
        return d2.exp - _DOUBLE_BIAS - _DOUBLE_PRECISION;
    }
    return d.exp - _DOUBLE_BIAS;
}
