#include <math.h>

float ldexpf(float x, int exp)
{
    if (exp == 0 || x == 0.0F || !isfinite(x))
    {
        return x;
    }

    _float_t scaleUp = {.val = 1.0F};
    scaleUp.exp += 100;

    _float_t scaleDown = {.val = 1.0F};
    scaleDown.exp -= 100;

    while (exp > 100)
    {
        x *= scaleUp.val;
        exp -= 100;
    }
    while (exp < -100)
    {
        x *= scaleDown.val;
        exp += 100;
    }

    _float_t factor = {.val = 1.0F};
    factor.exp += exp;
    return x * factor.val;
}
