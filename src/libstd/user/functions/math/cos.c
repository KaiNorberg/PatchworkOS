#include <errno.h>
#include <math.h>
#include <stdbool.h>

// https://cppreference.com/w/c/numeric/math/cos.html
double cos(double x)
{
    if (x == 0.0 || x == -0.0)
    {
        return 1.0;
    }

    if (isinf(x))
    {
        errno = EDOM;
        return NAN;
    }

    if (isnan(x))
    {
        return NAN;
    }

    double result;
    asm volatile("fcos" : "=t"(result) : "0"(x));
    return result;
}
