#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <sys/defs.h>

// https://cppreference.com/w/c/numeric/math/sin.html
double sin(double x)
{
    if (x == 0.0 || x == -0.0)
    {
        return 0.0;
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
    ASM("fsin" : "=t"(result) : "0"(x));
    return result;
}
