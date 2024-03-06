#pragma once

#include <stdint.h>

#define RFLAGS_INTERRUPT_ENABLE (1 << 9) 

extern uint64_t rflags_read();

extern void rflags_write(uint64_t rflags);