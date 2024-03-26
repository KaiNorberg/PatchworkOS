#pragma once

#include "trap_frame/trap_frame.h"
#include "sched/sched.h"

void sched_schedule(TrapFrame* trapFrame);

void sched_push(Thread* thread, uint8_t boost, uint16_t preferred);