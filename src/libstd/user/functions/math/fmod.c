#include <math.h>
#include <errno.h>

// https://en.cppreference.com/w/c/numeric/math/fmod
double fmod(double x, double y)
{
    if ((x == 0.0 || x == -0.0) && (y != 0.0 && y != -0.0))
    {
        return x;
    }

    if (isinf(x) && !isnan(y))
    {
        errno = EDOM;
        return NAN;
    }

    if ((y == 0.0 || y == -0.0) && !isnan(x))
    {
        errno = EDOM;
        return NAN;
    }

    if (isinf(y) && !isinf(x))
    {
        return x;
    }

    if (isnan(x) || isnan(y))
    {
        return NAN;
    }

    double quotient = trunc(x / y);
    double result = x - quotient * y;
    return result;
}
