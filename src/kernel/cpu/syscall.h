#pragma once

#include "trap.h"

extern void syscall_vector(void);

void syscall_handler(trap_frame_t* trapFrame);