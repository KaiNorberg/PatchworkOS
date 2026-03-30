#include <math.h>

double pow(double x, double y)
{
    if (x == 1.0 || y == 0.0)
    {
        return 1.0;
    }
    if (isnan(x) || isnan(y))
    {
        return NAN;
    }

    if (x < 0.0)
    {
        double int_part;
        if (modf(y, &int_part) != 0.0)
        {
            return NAN;
        }
        double res = exp2(y * log2(-x));
        if (fmod(int_part, 2.0) != 0.0)
        {
            res = -res;
        }
        return res;
    }

    if (x == 0.0)
    {
        if (y < 0.0)
        {
            return INFINITY;
        }
        return 0.0;
    }

    return exp2(y * log2(x));
}
