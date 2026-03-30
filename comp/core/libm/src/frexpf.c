#include <math.h>

float frexpf(float value, int* exp)
{
    if (value == 0.0F || isnan(value) || isinf(value))
    {
        *exp = 0;
        return value;
    }

    _float_t f = {.val = value};

    if (f.exp == 0)
    {
        value *= _FLOAT_MAX_INTEGER;
        f.val = value;
        *exp = f.exp - _FLOAT_BIAS - _FLOAT_PRECISION + 1;
    }
    else
    {
        *exp = f.exp - _FLOAT_BIAS + 1;
    }

    f.exp = _FLOAT_BIAS - 1;
    return f.val;
}
