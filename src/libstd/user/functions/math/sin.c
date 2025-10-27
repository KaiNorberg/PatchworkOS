#include <math.h>
#include <errno.h>
#include <stdbool.h>

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

    x = fmod(x, 2.0 * M_PI);
    bool negate = false;
    if (x < 0.0)
    {
        x = -x;
        negate = true;
    }

    if (x > M_PI)
    {
        x = 2.0 * M_PI - x;
        negate = !negate;
    }

    if (x > M_PI_2)
    {
        x = M_PI - x;
    }

    // sin(x) = x - x^3/3! + x^5/5! - x^7/7! + ...

    double x2 = x * x;
    double result = x;
    double term = x;
    for (int i = 1; i <= 10; i++)
    {
        term *= -x2 / ((2 * i) * (2 * i + 1));
        result += term;
        if (fabs(term) < 1e-16)
        {
            break;
        }
    }

    return negate ? -result : result;
}
