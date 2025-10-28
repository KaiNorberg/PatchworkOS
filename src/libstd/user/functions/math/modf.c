#include <math.h>
#include <stdint.h>

// https://cppreference.com/w/c/numeric/math/modf.html
double modf(double x, double* iptr)
{
    if (x == 0.0 || x == -0.0)
    {
        *iptr = x;
        return x;
    }

    if (isinf(x))
    {
        *iptr = x;
        return x == INFINITY ? 0.0 : -0.0;
    }

    if (isnan(x))
    {
        *iptr = NAN;
        return NAN;
    }

    double intPart;
    double fracPart = 0.0;
    if (x > 0.0)
    {
        intPart = floor(x);
        fracPart = x - intPart;
    }
    else
    {
        intPart = ceil(x);
        fracPart = x - intPart;
    }
    *iptr = intPart;
    return fracPart;
}
