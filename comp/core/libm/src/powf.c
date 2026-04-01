#include <math.h>

float powf(float x, float y)
{
    if (x == 1.0F || y == 0.0F)
    {
        return 1.0F;
    }
    if (isnan(x) || isnan(y))
    {
        return NAN;
    }

    if (x < 0.0F)
    {
        float int_part;
        if (modff(y, &int_part) != 0.0F)
        {
            return NAN;
        }
        float res = exp2f(y * log2f(-x));
        if (fmodf(int_part, 2.0F) != 0.0F)
        {
            res = -res;
        }
        return res;
    }

    if (x == 0.0F)
    {
        if (y < 0.0F)
        {
            return INFINITY;
        }
        return 0.0F;
    }

    return exp2f(y * log2f(x));
}
