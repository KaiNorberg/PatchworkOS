#include <math.h>

double modf(double value, double* iptr)
{
    _double_t d = {.val = value};
    int32_t exponent = d.exp - _DOUBLE_BIAS;

    if (exponent < 0)
    {
        *iptr = copysign(0.0, value);
        return value;
    }

    if (exponent >= _DOUBLE_BITS_MANTISSA)
    {
        *iptr = value;
        if (isnan(value))
        {
            return value;
        }
        return copysign(0.0, value);
    }

    uint64_t mask = -1ULL << (_DOUBLE_BITS_MANTISSA - exponent);
    _double_t intPart = d;
    intPart.mant &= mask;
    *iptr = intPart.val;

    return copysign(value - intPart.val, value);
}
