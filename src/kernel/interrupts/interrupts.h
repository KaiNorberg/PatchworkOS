#pragma once

#include "interrupt_frame/interrupt_frame.h"

void interrupts_disable(void);

void interrupts_enable(void);

void interrupt_handler(InterruptFrame* interruptFrame);