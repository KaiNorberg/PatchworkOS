#include <math.h>

float modff(float value, float* iptr)
{
    _float_t f = {.val = value};
    int32_t exponent = f.exp - _FLOAT_BIAS;

    if (exponent < 0)
    {
        *iptr = copysignf(0.0F, value);
        return value;
    }

    if (exponent >= _FLOAT_BITS_MANTISSA)
    {
        *iptr = value;
        if (isnan(value))
        {
            return value;
        }
        return copysignf(0.0F, value);
    }

    uint32_t mask = -1U << (_FLOAT_BITS_MANTISSA - exponent);
    _float_t intPart = f;
    intPart.mant &= mask;
    *iptr = intPart.val;

    return copysignf(value - intPart.val, value);
}
