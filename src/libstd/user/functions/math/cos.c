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

    x = fmod(x, 2.0 * M_PI);
    if (x < 0.0)
    {
        x = -x;
    }

    if (x > M_PI)
    {
        x = 2.0 * M_PI - x;
    }

    bool negate = false;
    if (x > M_PI / 2.0)
    {
        x = M_PI - x;
        negate = true;
    }

    // cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! + ...

    double x2 = x * x;
    double result = 1.0;
    double term = 1.0;
    for (int i = 1; i <= 10; i++)
    {
        term *= -x2 / ((2 * i - 1) * (2 * i));
        result += term;
        if (fabs(term) < 1e-16)
        {
            break;
        }
    }

    return negate ? -result : result;
}
