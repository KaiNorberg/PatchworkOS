#include <math.h>
#include <stdint.h>

double floor(double x)
{
    if (isinf(x) || x == 0.0 || x == -0.0)
    {
        return x;
    }

    if (isnan(x))
    {
        return NAN;
    }

    if (fabs(x) >= (double)INT64_MAX)
    {
        return x;
    }

    int64_t intPart = (int64_t)x;
    if (x < 0.0 && x != (double)intPart)
    {
        intPart--;
    }

    return (double)intPart;
}
