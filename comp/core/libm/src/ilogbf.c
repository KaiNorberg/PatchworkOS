#include <math.h>

int ilogbf(float x)
{
    if (x == 0.0F)
    {
        return FP_ILOGB0;
    }
    if (isnan(x) || isinf(x))
    {
        return FP_ILOGBNAN;
    }

    _float_t f = {.val = x};
    if (f.exp == 0)
    {
        _float_t f2 = {.val = x * _FLOAT_MAX_INTEGER};
        return f2.exp - _FLOAT_BIAS - _FLOAT_PRECISION;
    }
    return f.exp - _FLOAT_BIAS;
}
