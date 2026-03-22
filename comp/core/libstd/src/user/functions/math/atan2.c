#include <math.h>

// https://en.cppreference.com/w/c/numeric/math/atan2.html
double atan2(double y, double x)
{
    if (y == 0.0 || y == -0.0)
    {
        if (x < 0.0 || x == -0.0)
        {
            return (y < 0.0) ? -M_PI : M_PI;
        }
        else
        {
            return (y < 0.0) ? -0.0 : 0.0;
        }
    }

    if (isinf(y))
    {
        if (!isinf(x))
        {
            return (y < 0.0) ? -M_PI_2 : M_PI_2;
        }
        else if (x < 0.0)
        {
            return (y < 0.0) ? -3.0 * M_PI_4 : 3.0 * M_PI_4;
        }
        else
        {
            return (y < 0.0) ? -M_PI_4 : M_PI_4;
        }
    }

    if (x == 0.0 || x == -0.0)
    {
        return (y < 0.0) ? -M_PI_2 : M_PI_2;
    }

    if (isinf(x))
    {
        if (x < 0.0)
        {
            return (y < 0.0) ? -M_PI : M_PI;
        }
        else
        {
            return (y < 0.0) ? -0.0 : 0.0;
        }
    }

    if (isnan(x) || isnan(y))
    {
        return NAN;
    }

    double angle = atan(fabs(y / x));

    if (x > 0.0)
    {
        return (y < 0.0) ? -angle : angle;
    }
    else
    {
        return (y < 0.0) ? -M_PI + angle : M_PI - angle;
    }
}
