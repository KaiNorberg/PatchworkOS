#pragma once

#include "trap_frame/trap_frame.h"

void interrupts_disable(void);

void interrupts_enable(void);

void trap_handler(TrapFrame* trapFrame);