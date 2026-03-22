#include <sys/math.h>

uint64_t next_pow2(uint64_t n)
{
    if (n <= 1)
    {
        return 2;
    }

    if ((n > 0 && (n & (n - 1)) == 0))
    {
        return n;
    }

    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;

    if (n == 0)
    {
        return (UINT64_MAX >> 1) + 1;
    }

    return n;
}