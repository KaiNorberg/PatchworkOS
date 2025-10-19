#include <time.h>

#include "platform/user/common/clock.h"

clock_t clock(void)
{
    return _clock_get();
}
