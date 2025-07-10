#pragma once

#include "cpu/trap.h"
#include "defs.h"

NORETURN void panic(const trap_frame_t* trapFrame, const char* format, ...);
