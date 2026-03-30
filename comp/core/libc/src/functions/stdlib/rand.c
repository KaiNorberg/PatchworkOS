#include <stdlib.h>

#include "common/random.h"

int rand(void)
{
    return random_gen();
}
