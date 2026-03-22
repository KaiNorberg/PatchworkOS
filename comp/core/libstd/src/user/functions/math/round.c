#include <math.h>

// https://cppreference.com/w/c/numeric/math/round.html
double round(double x)
{
    if (isinf(x) || x == 0.0 || x == -0.0)
    {
        return x;
    }

    if (isnan(x))
    {
        return NAN;
    }

    double intPart;
    double fracPart = modf(x, &intPart);
    if (fabs(fracPart) < 0.5)
    {
        return intPart;
    }
    else if (fabs(fracPart) > 0.5)
    {
        return intPart + (x > 0.0 ? 1.0 : -1.0);
    }
    else
    {
        if (fmod(intPart, 2.0) == 0.0)
        {
            return intPart;
        }
        else
        {
            return intPart + (x > 0.0 ? 1.0 : -1.0);
        }
    }
}
