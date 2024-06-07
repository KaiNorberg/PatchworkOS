#pragma once

#include "defs.h"

#define XCR0_XSAVE_SAVE_X87 (1 << 0)
#define XCR0_XSAVE_SAVE_SSE (1 << 1)
#define XCR0_AVX_ENABLE (1 << 2)
#define XCR0_AVX512_ENABLE (1 << 5)
#define XCR0_ZMM0_15_ENABLE (1 << 6)
#define XCR0_ZMM16_32_ENABLE (1 << 7)

#define MSR_LOCAL_APIC 0x1B
#define MSR_CPU_ID 0xC0000103 //IA32_TSC_AUX

#define RFLAGS_ALWAYS_SET (1 << 1) 
#define RFLAGS_INTERRUPT_ENABLE (1 << 9) 

#define CR0_MONITOR_CO_PROCESSOR (1 << 1)
#define CR0_EMULATION (1 << 2)
#define CR0_NUMERIC_ERROR_ENABLE (1 << 5)

#define CR4_PAGE_GLOBAL_ENABLE (1 << 7)
#define CR4_FXSR_ENABLE (1 << 9)
#define CR4_SIMD_EXCEPTION (1 << 10)
#define CR4_XSAVE_ENABLE (1 << 18)

static inline void xcr0_write(uint32_t xcr, uint64_t value)
{
    uint32_t eax = (uint32_t)value;
    uint32_t edx = value >> 32;
    asm volatile("xsetbv" : : "a"(eax), "d"(edx), "c"(xcr) : "memory");
}

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

static inline uint64_t cr0_read() 
{
    uint64_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    return cr0;
}

static inline void cr0_write(uint64_t value) 
{
    asm volatile("mov %0, %%cr0" : : "r"(value));
}