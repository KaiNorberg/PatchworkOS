#pragma once

#include <stdint.h>

#define MSR_LOCAL_APIC 0x1B
#define MSR_CPU_ID 0xC0000103 //IA32_TSC_AUX

#define RFLAGS_ALWAYS_SET (1 << 1) 
#define RFLAGS_INTERRUPT_ENABLE (1 << 9) 

#define CR4_PAGE_GLOBAL_ENABLE (1 << 7)

uint64_t msr_read(uint64_t msr);
void msr_write(uint64_t msr, uint64_t value);

extern uint64_t rflags_read();
extern void rflags_write(uint64_t rflags);

extern uint64_t cr4_read();
extern void cr4_write(uint64_t value);

extern uint64_t cr3_read();
extern void cr3_write(uint64_t value);

extern uint64_t cr2_read();
extern void cr2_write(uint64_t value);
