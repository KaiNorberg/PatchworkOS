#pragma once

#define VECTOR_PAGE_FAULT 0xE

#define VECTOR_IRQ_BASE 0x20
#define VECTOR_NOTIFY 0x80
#define VECTOR_HALT 0x90
#define VECTOR_TIMER 0xA0

#define VECTOR_AMOUNT 256

extern void* vectorTable[VECTOR_AMOUNT];
