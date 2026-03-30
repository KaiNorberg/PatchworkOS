#include <math.h>

double frexp(double value, int* exp)
{
    if (value == 0.0 || isnan(value) || isinf(value))
    {
        *exp = 0;
        return value;
    }

    _double_t d = {.val = value};

    if (d.exp == 0)
    {
        value *= _DOUBLE_MAX_INTEGER;
        d.val = value;
        *exp = d.exp - _DOUBLE_BIAS - _DOUBLE_PRECISION + 1;
    }
    else
    {
        *exp = d.exp - _DOUBLE_BIAS + 1;
    }

    d.exp = _DOUBLE_BIAS - 1;
    return d.val;
}
