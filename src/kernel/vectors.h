#pragma once

#define VECTOR_IRQ_BASE 0x20
#define VECTOR_IPI 0x90
#define VECTOR_TIMER 0xA0
#define VECTOR_SCHED_SCHEDULE 0xB0
#define VECTOR_WAITSYS_BLOCK 0xC0

#define VECTOR_AMOUNT 256

extern void* vectorTable[VECTOR_AMOUNT];
