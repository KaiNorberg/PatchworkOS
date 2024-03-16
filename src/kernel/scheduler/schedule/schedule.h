#pragma once

#include "interrupt_frame/interrupt_frame.h"
#include "scheduler/scheduler.h"

void scheduler_schedule(InterruptFrame* interruptFrame);

void scheduler_push(Thread* thread, uint8_t boost, uint16_t preferred);