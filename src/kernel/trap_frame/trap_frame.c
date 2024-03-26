#include "trap_frame.h"

#include <string.h>

void trap_frame_copy(TrapFrame* dest, TrapFrame const* src)
{
    memcpy(dest, src, sizeof(TrapFrame));
}