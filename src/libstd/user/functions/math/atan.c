#include <math.h>
#include <stdbool.h>

// https://en.cppreference.com/w/c/numeric/math/atan.html
double atan(double x)
{
    if (x == 0.0 || x == -0.0)
    {
        return x;
    }

    if (isinf(x))
    {
        return (x > 0.0) ? M_PI_2 : -M_PI_2;
    }

    if (isnan(x))
    {
        return NAN;
    }

    bool negate = 0;
    if (x < 0.0)
    {
        x = -x;
        negate = true;
    }

    bool invert = 0;
    if (x > 1.0)
    {
        x = 1.0 / x;
        invert = true;
    }

    // atan(x) = x - x^3/3 + x^5/5 - x^7/7 + ...

    double x2 = x * x;
    double result = 0.0;
    double term = x;
    int sign = 1;
    for (int i = 1; i <= 100; i += 2)
    {
        result += sign * term / i;
        term *= x2;
        sign = -sign;
        if (fabs(term / i) < 1e-16)
        {
            break;
        }
    }

    if (invert)
    {
        result = M_PI_2 - result;
    }

    return negate ? -result : result;
}
