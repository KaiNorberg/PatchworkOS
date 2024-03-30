#pragma once

#include "trap/trap.h"
#include "sched/sched.h"

void sched_schedule(TrapFrame* trapFrame);

void sched_push(Thread* thread, uint8_t boost);