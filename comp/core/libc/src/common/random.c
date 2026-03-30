#include "random.h"

#include <stdint.h>
#include <stdlib.h>

static uint64_t seed = 0;

int random_gen(void)
{
    uint64_t oldstate = seed;

    seed = oldstate * 6364136223846793005ULL + 1442695040888963407ULL;

    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    uint32_t result = (xorshifted >> rot) | (xorshifted << ((-rot) & 31));

    return (int)(result & RAND_MAX);
}

void random_seed(unsigned newSeed)
{
    seed = newSeed * 6364136223846793005ULL + 1442695040888963407ULL;
}
