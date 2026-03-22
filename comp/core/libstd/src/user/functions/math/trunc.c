#include <math.h>

double trunc(double x)
{
    if (isinf(x))
    {
        return x;
    }

    if (x == 0.0 || x == -0.0)
    {
        return x;
    }

    if (isnan(x))
    {
        return NAN;
    }

    if (x > 0)
    {
        return floor(x);
    }
    else
    {
        return ceil(x);
    }
}
