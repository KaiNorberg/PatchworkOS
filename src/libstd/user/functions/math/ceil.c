#include <math.h>
#include <stdint.h>

// https://en.cppreference.com/w/c/numeric/math/ceil
double ceil(double x)
{
    if (isinf(x) || x == 0.0 || x == -0.0)
    {
        return x;
    }

    if (isnan(x))
    {
        return NAN;
    }

    if (fabs(x) > INT64_MAX)
    {
        return x;
    }

    int64_t intPart = (int64_t)x;
    if (x > 0.0 && (double)intPart < x)
    {
        intPart++;
    }

    return (double)intPart;
}
