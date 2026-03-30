#include <math.h>

double ldexp(double x, int exp)
{
    if (exp == 0 || x == 0.0 || !isfinite(x))
    {
        return x;
    }

    _double_t scaleUp = {.val = 1.0};
    scaleUp.exp += 1000;

    _double_t scaleDown = {.val = 1.0};
    scaleDown.exp -= 1000;

    while (exp > 1000)
    {
        x *= scaleUp.val;
        exp -= 1000;
    }
    while (exp < -1000)
    {
        x *= scaleDown.val;
        exp += 1000;
    }

    _double_t factor = {.val = 1.0};
    factor.exp += exp;
    return x * factor.val;
}
