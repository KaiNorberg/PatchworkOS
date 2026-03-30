#include <math.h>

float logbf(float x)
{
    if (x == 0.0F)
    {
        return -INFINITY;
    }
    if (isnan(x) || isinf(x))
    {
        return x * x;
    }

    _float_t f = {.val = x};
    if (f.exp == 0)
    {
        _float_t f2 = {.val = x * _FLOAT_MAX_INTEGER};
        return (float)(f2.exp - _FLOAT_BIAS - _FLOAT_PRECISION);
    }
    return (float)(f.exp - _FLOAT_BIAS);
}
