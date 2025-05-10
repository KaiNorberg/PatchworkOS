#include <stdlib.h>

#include "common/random.h"

void srand(unsigned seed)
{
    random_seed(seed);
}
