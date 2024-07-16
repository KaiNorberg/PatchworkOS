#pragma once

#define VECTOR_IPI 0x90
#define VECTOR_SCHED_TIMER 0xA0
#define VECTOR_SCHED_INVOKE 0xB0

#define VECTOR_AMOUNT 256

extern void* vectorTable[VECTOR_AMOUNT];
