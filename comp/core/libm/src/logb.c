#include <math.h>

double logb(double x)
{
    if (x == 0.0)
    {
        return -INFINITY;
    }
    if (isnan(x) || isinf(x))
    {
        return x * x;
    }

    _double_t d = {.val = x};
    if (d.exp == 0)
    {
        _double_t d2 = {.val = x * _DOUBLE_MAX_INTEGER};
        return (double)(d2.exp - _DOUBLE_BIAS - _DOUBLE_PRECISION);
    }
    return (double)(d.exp - _DOUBLE_BIAS);
}
