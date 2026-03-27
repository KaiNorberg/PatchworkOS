#include <math.h>

float roundf(float x)
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
    float fracPart = modf(x, &intPart);
    if (fabs(fracPart) < 0.5)
    {
        return intPart;
    }

    if (fabs(fracPart) > 0.5)
    {
        return intPart + (x > 0.0 ? 1.0 : -1.0);
    }

    if (fmod(intPart, 2.0) == 0.0)
    {
        return intPart;
    }

    return intPart + (x > 0.0 ? 1.0 : -1.0);
}
