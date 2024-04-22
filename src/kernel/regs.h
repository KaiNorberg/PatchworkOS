#pragma once

#include "defs.h"

#define MSR_LOCAL_APIC 0x1B
#define MSR_CPU_ID 0xC0000103 //IA32_TSC_AUX

#define RFLAGS_ALWAYS_SET (1 << 1) 
#define RFLAGS_INTERRUPT_ENABLE (1 << 9) 

#define CR4_PAGE_GLOBAL_ENABLE (1 << 7)

static inline uint64_t msr_read(uint32_t msr) 
{
    uint32_t low;
    uint32_t high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | (uint64_t)low;
}

static inline void msr_write(uint32_t msr, uint64_t value) 
{
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rflags_read() 
{
    uint64_t rflags;
    asm volatile("pushfq; pop %0" : "=r"(rflags));
    return rflags;
}

static inline void rflags_write(uint64_t value) 
{
    asm volatile("push %0; popfq" : : "r"(value));
}

static inline uint64_t cr4_read() 
{
    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    return cr4;
}

static inline void cr4_write(uint64_t value) 
{
    asm volatile("mov %0, %%cr4" : : "r"(value));
}

static inline uint64_t cr3_read() 
{
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static inline void cr3_write(uint64_t value) 
{
    asm volatile("mov %0, %%cr3" : : "r"(value));
}

static inline uint64_t cr2_read() 
{
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

static inline void cr2_write(uint64_t value) 
{
    asm volatile("mov %0, %%cr2" : : "r"(value));
}