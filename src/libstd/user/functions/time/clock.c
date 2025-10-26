#include <time.h>

#include "user/common/clock.h"

clock_t clock(void)
{
    return _clock_get();
}
